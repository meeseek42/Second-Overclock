import { resolveInput } from "../shared/paths.mjs";
import crypto from "node:crypto";
import fs from "node:fs";
import { files } from "../shared/paths.mjs";
import { readJson, writeJson, writeText } from "../shared/io.mjs";
import { loadGF02Inputs, overlayGF02Recipes } from "./gf02-contract.mjs";
import { loadGF04Inputs, overlayGF04Recipes } from "./gf04-contract.mjs";
import { loadGF03Inputs, overlayGF03Recipes } from "./gf03-contract.mjs";
import {loadRemainingInputs,overlayRemainingRecipes,appendRemainingCatalogRows} from "./remaining-contract.mjs";
import { selectNumeric } from "./native-numeric-policy.mjs";

const SUPPORTED_COMPLEXITY_MAX_EXCLUSIVE = 4;
const BRIDGE_HANDLERS = new Map([
  ["52719af9436041bb", "goo_bomber_disperser_v1"],
  ["37b17eb4809a2ce6", "linecutter_returning_roll_v1"],
]);

// Owner-reviewed playability exceptions. These are deliberately keyed by
// stable pair id and exact leaf path: the first overclock keeps its original
// drawback, while only the second overclock's matching drawback contributes
// half of its distance from the neutral value.
const SECOND_PENALTY_HALF = new Map([
  ["96f1db6528f01103",
    "/Game/WeaponsNTools/AssaultRifle/Overclocks/OC_BonusesAndPenalties/" +
    "OC_Penalty_RoF-2_AssaultRifle.OC_Penalty_RoF-2_AssaultRifle"],
  ["8fe309ec5b8d3f40",
    "/Game/WeaponsNTools/GrenadeLauncher/Overclocks/OC_BonusAndPenalty/" +
    "Penalty_Ammo-2_PGL.Penalty_Ammo-2_PGL"],
  ["d1149a2af5dadca6",
    "/Game/WeaponsNTools/LineCutter/Overclocks/OC_BonusesAndPenalties/" +
    "Penalty_ClipsizeBig_Linecutter.Penalty_ClipsizeBig_Linecutter"],
]);
const SHARED_SECOND_PENALTY_HALF = new Set(["bccaf9883c8f7f6c"]);

function stableJson(value) {
  if (Array.isArray(value)) return `[${value.map(stableJson).join(",")}]`;
  if (value && typeof value === "object") {
    return `{${Object.keys(value).sort().map((key) =>
      `${JSON.stringify(key)}:${stableJson(value[key])}`).join(",")}}`;
  }
  return JSON.stringify(value);
}

function sha256(value) {
  return crypto.createHash("sha256").update(value).digest("hex");
}

function csvCell(value) {
  const text = String(value ?? "");
  return /[",\r\n]/.test(text) ? `"${text.replaceAll('"', '""')}"` : text;
}

function packagePathToObjectPath(packagePath) {
  const leaf = packagePath.split("/").at(-1);
  return `${packagePath}.${leaf}`;
}

function uniqueInOrder(values) {
  const seen = new Set();
  return values.filter((value) => {
    if (seen.has(value)) return false;
    seen.add(value);
    return true;
  });
}

function overclockCategoryFromParsedAsset(overclock, parsedIndex) {
  const indexRow = parsedIndex.rows.find((row) => row.packagePath === overclock.rootAsset);
  if (!indexRow?.parsedJson || !fs.existsSync(resolveInput(indexRow.parsedJson))) throw Error(`Missing parsed category input: ${overclock.rootAsset}`);
  const parsed = readJson(indexRow.parsedJson);
  const importText = JSON.stringify(parsed.Imports ?? []);
  if (importText.includes("SCAT_OC_Clean")) return "clean";
  if (importText.includes("SCAT_OC_Balanced")) return "balanced";
  if (importText.includes("SCAT_OC_Unstable")) return "unstable";
  throw Error(`Unrecognized overclock category: ${overclock.rootAsset}`);
}

function buildOverclockRows(catalog, implementations, parsedIndex) {
  const sourceByPath = new Map(catalog.weapons.flatMap((weapon) =>
    weapon.overclocks.map((oc) => [oc.objectPath, { weapon, oc }])));

  return implementations.overclocks.map((implementation) => {
    const source = sourceByPath.get(implementation.objectPath);
    if (!source) throw new Error(`Catalog row missing for ${implementation.objectPath}`);
    return {
      overclockId: implementation.overclockId,
      classKey: implementation.classKey,
      slot: implementation.slot,
      weaponFolder: implementation.assetFolder,
      weaponNameZh: implementation.weaponName,
      weaponObjectPath: source.weapon.weaponObjectPath,
      weaponIdObjectPath: source.weapon.idObjectPath,
      nameZh: implementation.nameZh,
      nameEn: implementation.nameEn,
      effectZh: source.oc.effectsZh,
      objectPath: implementation.objectPath,
      rootAsset: implementation.rootAsset,
      leafObjectPaths: implementation.combinedUpgradeOrder.map(packagePathToObjectPath),
      category: overclockCategoryFromParsedAsset(implementation, parsedIndex),
      evidenceStatus: implementation.gate.status,
    };
  }).sort((a, b) =>
    a.weaponFolder.localeCompare(b.weaponFolder, "en") ||
    a.nameZh.localeCompare(b.nameZh, "zh-CN"));
}

function buildLeafMergeInfo(implementations) {
  const result = new Map();
  for (const overclock of implementations.overclocks) {
    for (const leaf of overclock.leaves) {
      const objectPath = packagePathToObjectPath(leaf.packagePath);
      const amountProperty = leaf.properties.find((property) => property.name === "Amount");
      if (!amountProperty || typeof amountProperty.value !== "number") continue;
      const className = leaf.classNames.find((name) =>
        name !== "/Script/FSD" && !name.startsWith("Default__"));
      if (!className) throw new Error(`Native upgrade class missing for ${objectPath}`);
      const percentageProperty =
        leaf.properties.find((property) => property.name === "IsPercentage");
      const upgradeTypeProperty =
        leaf.properties.find((property) => property.name === "UpgradeType");
      const info = {
        sourceObjectPath: objectPath,
        sourceClassPath: `/Script/FSD.${className}`,
        amount: amountProperty.value,
        isPercentage: percentageProperty?.value === true,
        upgradeType: typeof upgradeTypeProperty?.value === "string"
          ? upgradeTypeProperty.value
          : "",
      };
      const existing = result.get(objectPath);
      if (existing && stableJson(existing) !== stableJson(info)) {
        throw new Error(`Conflicting merge metadata for ${objectPath}`);
      }
      result.set(objectPath, info);
    }
  }
  return result;
}

function mergedLeafObjectPath(assessment, duplicateIndex) {
  const assetName =
    `DORMerged_${assessment.pairId}_${String(duplicateIndex + 1).padStart(2, "0")}`;
  return `/Game/DualOverclockReward/MergedLeaves/${assessment.assetFolder}/` +
    `${assetName}.${assetName}`;
}

function adjustedLeafObjectPath(assessment, adjustedIndex) {
  const assetName =
    `DORAdjusted_${assessment.pairId}_${String(adjustedIndex + 1).padStart(2, "0")}`;
  return `/Game/DualOverclockReward/AdjustedLeaves/${assessment.assetFolder}/` +
    `${assetName}.${assetName}`;
}

function numericUtilityFor(info, numericPolicy) {
  return numericPolicy.targetUtility?.[info.upgradeType] ??
    numericPolicy.leafUtility?.[info.sourceObjectPath] ?? null;
}

function percentageMergeCandidates(amount, contributionCount, laterContributionScale) {
  const laterAmount = 1 + laterContributionScale * (amount - 1);
  return {
    additive: 1 + (amount - 1) +
      (contributionCount - 1) * laterContributionScale * (amount - 1),
    multiplicative: amount * (laterAmount ** (contributionCount - 1)),
  };
}

function classifyPercentagePolarity(amount, utility, context) {
  if (!["higher-is-better", "lower-is-better"].includes(utility)) {
    throw new Error(`Numeric utility direction requires manual review for ${context}`);
  }
  if (Math.abs(amount - 1) < 0.000001) return "neutral";
  const beneficial = utility === "higher-is-better" ? amount > 1 : amount < 1;
  return beneficial ? "positive" : "negative";
}

function chooseRiskControlledCandidate(candidates, utility, sourceAmounts, context) {
  const polarities = sourceAmounts
    .map((amount) => classifyPercentagePolarity(amount, utility, context))
    .filter((polarity) => polarity !== "neutral");
  const uniquePolarities = new Set(polarities);
  const effectPolarity = uniquePolarities.size > 1 ? "mixed" : (polarities[0] ?? "neutral");
  const legal = Object.entries(candidates).filter(([, value]) => Number.isFinite(value) && value >= 0);
  if (!legal.length) throw new Error(`No legal numeric candidate for ${context}`);
  const chooseHigher = effectPolarity !== "positive"
    ? utility === "higher-is-better"
    : utility === "lower-is-better";
  const additiveLegal = Number.isFinite(candidates.additive) && candidates.additive >= 0;
  const multiplicativeLegal = Number.isFinite(candidates.multiplicative) && candidates.multiplicative >= 0;
  const multiplicativeWins = multiplicativeLegal && (!additiveLegal || (chooseHigher
    ? candidates.multiplicative > candidates.additive
    : candidates.multiplicative < candidates.additive));
  return {
    mode: multiplicativeWins ? "multiplicative" : "additive-deviation-from-neutral",
    value: multiplicativeWins ? candidates.multiplicative : candidates.additive,
    effectPolarity,
    selectionRule: effectPolarity === "negative"
      ? "player-favorable-for-double-negative"
      : effectPolarity === "positive" ? "player-unfavorable-for-double-positive"
        : "player-favorable-for-negative-or-mixed",
  };
}

function buildPairRows(pairAssessments, overclocks, leafMergeInfo, numericPolicy, mirrorConfig) {
  const overclockById = new Map(overclocks.map((row) => [row.overclockId, row]));
  const mirrorByPair = new Map((mirrorConfig.weapons ?? []).map((row) => [row.pairId, row]));
  const supported = pairAssessments.assessments
    .filter((row) => row.scores.implementationComplexity < SUPPORTED_COMPLEXITY_MAX_EXCLUSIVE)
    .sort((a, b) =>
      a.assetFolder.localeCompare(b.assetFolder, "en") ||
      a.ocA.nameZh.localeCompare(b.ocA.nameZh, "zh-CN") ||
      a.ocB.nameZh.localeCompare(b.ocB.nameZh, "zh-CN"));

  return supported.map((assessment, index) => {
    const ocA = overclockById.get(assessment.ocA.overclockId);
    const ocB = overclockById.get(assessment.ocB.overclockId);
    if (!ocA || !ocB) throw new Error(`Implementation missing for pair ${assessment.pairId}`);
    if (ocA.weaponFolder !== ocB.weaponFolder) {
      throw new Error(`Cross-weapon pair ${assessment.pairId}`);
    }

    const ordered = [...ocA.leafObjectPaths, ...ocB.leafObjectPaths];
    const deduplicated = uniqueInOrder(ordered);
    const duplicateLeafObjectPaths = uniqueInOrder(
      ordered.filter((value, itemIndex) => ordered.indexOf(value) !== itemIndex),
    );
    const mergedSharedLeaves = duplicateLeafObjectPaths.map((sourceObjectPath, duplicateIndex) => {
      const info = leafMergeInfo.get(sourceObjectPath);
      if (!info) {
        throw new Error(
          `Shared leaf ${sourceObjectPath} in ${assessment.pairId} has no numeric merge metadata`,
        );
      }
      const contributionCount = ordered.filter((value) => value === sourceObjectPath).length;
      if (contributionCount < 2) {
        throw new Error(`Invalid shared leaf count for ${sourceObjectPath}`);
      }
      const laterContributionScale =
        SHARED_SECOND_PENALTY_HALF.has(assessment.pairId) ? 0.5 : 1;
      const effectiveContributionCount =
        1 + (contributionCount - 1) * laterContributionScale;
      const utilityDirection = info.isPercentage
        ? numericUtilityFor(info, numericPolicy)
        : null;
      const percentageCandidates = info.isPercentage
        ? percentageMergeCandidates(
          info.amount,
          contributionCount,
          laterContributionScale,
        )
        : null;
      const selected = info.isPercentage
        ? chooseRiskControlledCandidate(
          percentageCandidates,
          utilityDirection,
          Array(contributionCount).fill(info.amount),
          `${assessment.pairId}:${sourceObjectPath}`,
        )
        : {
          mode: "flat-additive",
          value: effectiveContributionCount * info.amount,
          effectPolarity: null,
          selectionRule: "flat-additive",
        };
      return {
        sourceObjectPath,
        sourceClassPath: info.sourceClassPath,
        mergedObjectPath: mergedLeafObjectPath(assessment, duplicateIndex),
        sourceAmount: info.amount,
        isPercentage: info.isPercentage,
        upgradeType: info.upgradeType,
        contributionCount,
        laterContributionScale,
        utilityDirection,
        additiveCandidate: percentageCandidates?.additive ?? null,
        multiplicativeCandidate: percentageCandidates?.multiplicative ?? null,
        mergeMode: selected.mode,
        mergedAmount: selected.value,
        effectPolarity: selected.effectPolarity,
        selectionRule: selected.selectionRule,
      };
    });
    const runtimeMergeDecisions = (numericPolicy.pairOverrides?.[assessment.pairId] ?? [])
      .filter((decision) => decision.runtimeMerge === true);
    for (const [runtimeIndex, decision] of runtimeMergeDecisions.entries()) {
      const infos = decision.sourceObjectPaths.map((sourceObjectPath) => {
        if (!ordered.includes(sourceObjectPath)) {
          throw new Error(
            `Runtime numeric merge leaf ${sourceObjectPath} is absent from ${assessment.pairId}`,
          );
        }
        const info = leafMergeInfo.get(sourceObjectPath);
        if (!info || info.upgradeType !== decision.target) {
          throw new Error(
            `Runtime numeric merge target mismatch for ${assessment.pairId}:${sourceObjectPath}`,
          );
        }
        return info;
      });
      if (infos.length < 2 || new Set(decision.sourceObjectPaths).size !== infos.length) {
        throw new Error(`Runtime numeric merge needs distinct leaves for ${assessment.pairId}`);
      }
      if (!infos.every((info) => info.isPercentage === infos[0].isPercentage)) {
        throw new Error(`Runtime numeric merge percentage mismatch for ${assessment.pairId}`);
      }
      const additiveCandidate = infos[0].isPercentage
        ? 1 + infos.reduce((sum, info) => sum + info.amount - 1, 0)
        : infos.reduce((sum, info) => sum + info.amount, 0);
      const multiplicativeCandidate = infos[0].isPercentage
        ? infos.reduce((product, info) => product * info.amount, 1)
        : null;
      if (typeof decision.mergedAmount !== "number") {
        throw new Error(`Runtime numeric merge amount missing for ${assessment.pairId}`);
      }
      if (decision.mode === "additive-deviation-from-neutral" &&
          Math.abs(decision.mergedAmount - additiveCandidate) > 0.000001) {
        throw new Error(`Runtime additive merge mismatch for ${assessment.pairId}`);
      }
      if (decision.mode === "flat-additive" &&
          (infos[0].isPercentage ||
           Math.abs(decision.mergedAmount - additiveCandidate) > 0.000001)) {
        throw new Error(`Runtime flat merge mismatch for ${assessment.pairId}`);
      }
      if (decision.mode === "strongest-single" &&
          !infos.some((info) => Math.abs(info.amount - decision.mergedAmount) < 0.000001)) {
        throw new Error(`Runtime strongest-single merge mismatch for ${assessment.pairId}`);
      }
      const policySelection = decision.nativeOperatorProvenance
        ? (() => {
          const selected = selectNumeric(infos.map(info => info.amount), infos[0].isPercentage,
            decision.utility, decision.nativeOperatorProvenance.operator);
          return {...selected, selectionRule: selected.rule};
        })()
        : infos[0].isPercentage &&
        ["additive-deviation-from-neutral", "multiplicative"].includes(decision.mode)
        ? chooseRiskControlledCandidate(
          { additive: additiveCandidate, multiplicative: multiplicativeCandidate },
          decision.utility,
          infos.map((info) => info.amount),
          `${assessment.pairId}:${decision.target}`,
        )
        : null;
      if (policySelection &&
          (policySelection.mode !== decision.mode ||
           Math.abs(policySelection.value - decision.mergedAmount) > 0.000001)) {
        throw new Error(`Runtime merge violates polarity policy for ${assessment.pairId}`);
      }
      mergedSharedLeaves.push({
        sourceObjectPath: decision.sourceObjectPaths[0],
        sourceObjectPaths: [...decision.sourceObjectPaths],
        sourceClassPath: infos[0].sourceClassPath,
        mergedObjectPath: mergedLeafObjectPath(
          assessment,
          duplicateLeafObjectPaths.length + runtimeIndex,
        ),
        sourceAmount: infos[0].amount,
        sourceAmounts: infos.map((info) => info.amount),
        isPercentage: infos[0].isPercentage,
        upgradeType: decision.target,
        contributionCount: infos.length,
        laterContributionScale: 1,
        utilityDirection: decision.utility,
        additiveCandidate,
        multiplicativeCandidate,
        mergeMode: decision.mode,
        mergedAmount: decision.mergedAmount,
        effectPolarity: policySelection?.effectPolarity ?? null,
        selectionRule: policySelection?.selectionRule ?? decision.mode,
      });
    }
    const mergedBySource = new Map();
    for (const leaf of mergedSharedLeaves) {
      for (const sourceObjectPath of leaf.sourceObjectPaths ?? [leaf.sourceObjectPath]) {
        if (mergedBySource.has(sourceObjectPath)) {
          throw new Error(`Numeric leaf merged more than once: ${sourceObjectPath}`);
        }
        mergedBySource.set(sourceObjectPath, leaf.mergedObjectPath);
      }
    }
    const adjustedSourcePath = SECOND_PENALTY_HALF.get(assessment.pairId);
    const adjustedLeaves = [];
    if (adjustedSourcePath) {
      if (!ocB.leafObjectPaths.includes(adjustedSourcePath)) {
        throw new Error(
          `Second-penalty override ${assessment.pairId} does not target an OC-B leaf`,
        );
      }
      const info = leafMergeInfo.get(adjustedSourcePath);
      if (!info) {
        throw new Error(
          `Second-penalty override ${adjustedSourcePath} has no numeric metadata`,
        );
      }
      adjustedLeaves.push({
        sourceObjectPath: adjustedSourcePath,
        sourceClassPath: info.sourceClassPath,
        adjustedObjectPath: adjustedLeafObjectPath(assessment, 0),
        sourceAmount: info.amount,
        isPercentage: info.isPercentage,
        upgradeType: info.upgradeType,
        penaltyScale: 0.5,
        adjustedAmount: info.isPercentage
          ? 1 + 0.5 * (info.amount - 1)
          : 0.5 * info.amount,
      });
    }
    const adjustedBySource = new Map(
      adjustedLeaves.map((leaf) => [leaf.sourceObjectPath, leaf.adjustedObjectPath]),
    );
    const emitted = new Set();
    const emittedMerged = new Set();
    const finalLeafObjectPaths = [];
    for (const leafPath of ordered) {
      if (emitted.has(leafPath)) continue;
      emitted.add(leafPath);
      const mergedPath = mergedBySource.get(leafPath);
      if (mergedPath) {
        if (!emittedMerged.has(mergedPath)) {
          emittedMerged.add(mergedPath);
          finalLeafObjectPaths.push(mergedPath);
        }
      } else {
        finalLeafObjectPaths.push(adjustedBySource.get(leafPath) ?? leafPath);
      }
    }
    const bridgeHandlerId = BRIDGE_HANDLERS.get(assessment.pairId) ?? "";
    if (assessment.route === "轻量桥接" && !bridgeHandlerId) {
      throw new Error(`Bridge pair ${assessment.pairId} has no handler`);
    }
    const numericStackDecisions = (numericPolicy.pairOverrides?.[assessment.pairId] ?? [])
      .map((decision) => {
        if (!["higher-is-better", "lower-is-better"].includes(decision.utility)) {
          throw new Error(`Invalid numeric utility for ${assessment.pairId}:${decision.target}`);
        }
        if (!Array.isArray(decision.sourceObjectPaths) || decision.sourceObjectPaths.length < 2) {
          throw new Error(`Numeric override needs at least two leaves for ${assessment.pairId}`);
        }
        for (const sourceObjectPath of decision.sourceObjectPaths) {
          if (!ordered.includes(sourceObjectPath)) {
            throw new Error(
              `Numeric override leaf ${sourceObjectPath} is absent from ${assessment.pairId}`,
            );
          }
          const info = leafMergeInfo.get(sourceObjectPath);
          if (!info || info.upgradeType !== decision.target) {
            throw new Error(
              `Numeric override target mismatch for ${assessment.pairId}:${sourceObjectPath}`,
            );
          }
        }
        return { ...decision };
      });

    for (const decision of numericStackDecisions.filter(d => d.mode === "percentage-then-flat")) {
      const sources = decision.sourceObjectPaths.map(p => ({path:p, info:leafMergeInfo.get(p)}));
      const factor = sources.find(s => s.info.isPercentage), flat = sources.find(s => !s.info.isPercentage);
      if (!factor || !flat || sources.length !== 2) throw Error("Mixed native order requires one ADD and one MUL");
      const indices = sources.map(s => finalLeafObjectPaths.indexOf(s.path)).sort((a,b) => a-b);
      if (indices[0] < 0) throw Error("Mixed native leaf missing");
      finalLeafObjectPaths[indices[0]] = factor.path;
      finalLeafObjectPaths[indices[1]] = flat.path;
    }
    const mirror = mirrorByPair.get(assessment.pairId) ?? null;
    const pairUpgradeObjectPath =
      `/Game/DualOverclockReward/Pairs/${assessment.assetFolder}/` +
      `DORPair_${String(index + 1).padStart(3, "0")}_${assessment.pairId}.` +
      `DORPair_${String(index + 1).padStart(3, "0")}_${assessment.pairId}`;
    if (mirror && (mirror.testIndex !== index + 1 || mirror.fusionRootPath !== pairUpgradeObjectPath)) {
      throw new Error(`Weapon mirror config does not match runtime row ${assessment.pairId}`);
    }
    const row = {
      pairId: assessment.pairId,
      testIndex: index + 1,
      classKey: ocA.classKey,
      classNameZh: assessment.className,
      slot: assessment.slot,
      weaponFolder: assessment.assetFolder,
      weaponNameZh: assessment.weaponName,
      weaponObjectPath: ocA.weaponObjectPath,
      weaponIdObjectPath: ocA.weaponIdObjectPath,
      pairUpgradeObjectPath,
      weaponMirrorClassPath: mirror
        ? `${mirror.mirrorWeaponPackage}.${mirror.mirrorWeaponObject}_C`
        : "",
      weaponMirrorSavegameId: mirror?.mirrorSavegameId ?? "",
      weaponMirrorItemIdPath: mirror?.mirrorItemIdPath ?? "",
      nativeItemIdPath: mirror?.nativeItemIdPath ?? "",
      fusionRootPath: mirror?.fusionRootPath ?? pairUpgradeObjectPath,
      pairSpecificWeaponClassPath: "",
      overclockAId: ocA.overclockId,
      overclockANameZh: ocA.nameZh,
      overclockANameEn: ocA.nameEn,
      overclockAEffectZh: ocA.effectZh,
      overclockAObjectPath: ocA.objectPath,
      overclockBId: ocB.overclockId,
      overclockBNameZh: ocB.nameZh,
      overclockBNameEn: ocB.nameEn,
      overclockBEffectZh: ocB.effectZh,
      overclockBObjectPath: ocB.objectPath,
      orderedLeafObjectPaths: ordered,
      finalLeafObjectPaths,
      duplicateLeafObjectPaths,
      mergedSharedLeaves,
      adjustedLeaves,
      numericStackDecisions,
      sharedWriteTargets: assessment.collision.sharedWrites ?? [],
      route: assessment.route,
      feasibility: assessment.feasibility,
      implementationComplexity: assessment.scores.implementationComplexity,
      bridgeHandlerId,
      implementationNameZh: assessment.recommendation.nameZh,
      implementationSummaryZh: assessment.recommendation.funChangeZh,
      protectionBoundaryZh: assessment.recommendation.protectionBoundaryZh,
    };
    return { ...row, rowHash: sha256(stableJson(row)) };
  });
}

function assertManifest(manifest) {
  const pairs = manifest.pairs;
  if (manifest.overclocks.length !== 160) throw new Error("Expected 160 overclocks");
  if (pairs.length !== 456) throw new Error(`Expected 456 supported pairs, got ${pairs.length}`);
  if (new Set(pairs.map((row) => row.pairId)).size !== 456) throw new Error("Duplicate pairId");
  if (new Set(pairs.map((row) => row.testIndex)).size !== 456) throw new Error("Duplicate testIndex");
  const routeCounts = Object.groupBy
    ? Object.fromEntries(Object.entries(Object.groupBy(pairs, (row) => row.route))
      .map(([key, values]) => [key, values.length]))
    : pairs.reduce((result, row) => {
      result[row.route] = (result[row.route] ?? 0) + 1;
      return result;
    }, {});
  if (routeCounts["直接叠加"] !== 438 ||
      routeCounts["顺序保护"] !== 12 ||
      routeCounts["轻量桥接"] !== 2 || routeCounts["定制融合"] !== 4) {
    throw new Error(`Unexpected route counts: ${JSON.stringify(routeCounts)}`);
  }
  for (const row of pairs) {
    if (!row.remainingPrepared && row.implementationComplexity >= SUPPORTED_COMPLEXITY_MAX_EXCLUSIVE) {
      throw new Error(`Unsupported complexity leaked into ${row.pairId}`);
    }
    if (!row.finalLeafObjectPaths.length) throw new Error(`No leaves for ${row.pairId}`);
    if (!row.gf02 && !row.gf03 && !row.remainingPrepared && new Set(row.finalLeafObjectPaths).size !== row.finalLeafObjectPaths.length) {
      throw new Error(`Duplicate final leaf for ${row.pairId}`);
    }
    for (const merged of row.mergedSharedLeaves) {
      if (!row.finalLeafObjectPaths.includes(merged.mergedObjectPath) ||
          (!row.gf02 && !row.gf03 && !row.remainingPrepared && (merged.sourceObjectPaths ?? [merged.sourceObjectPath])
            .some((sourceObjectPath) => row.finalLeafObjectPaths.includes(sourceObjectPath)))) {
        throw new Error(`Merged shared leaf was not substituted for ${row.pairId}`);
      }
    }
  }
  const mergedPairs = pairs.filter((row) => row.mergedSharedLeaves.length);
  const mergedLeaves = mergedPairs.flatMap((row) => row.mergedSharedLeaves);
  if (mergedLeaves.length !== 195 || pairs.filter((row) => row.gf02).length !== 105 ||
      pairs.filter((row) => row.gf04).length !== 63 ||
      pairs.filter((row) => row.gf03).length !== 79 ||
      pairs.flatMap((row) => row.adjustedLeaves).length !== 43) {
    throw new Error(
      `Expected GF02 105/GF03 79/GF04 63 rows/195 merged leaves/43 adjusted leaves, got ` +
      `${mergedPairs.length}/${mergedLeaves.length}`,
    );
  }
}

function buildRuntimeCsv(pairs) {
  const columns = [
    "Name", "pairId", "testIndex", "classKey", "classNameZh", "slot",
    "weaponFolder", "weaponNameZh", "weaponObjectPath", "weaponIdObjectPath",
    "pairUpgradeObjectPath", "weaponMirrorClassPath", "weaponMirrorSavegameId",
    "nativeItemIdPath", "fusionRootPath", "pairSpecificWeaponClassPath",
    "overclockAId", "overclockANameZh", "overclockANameEn", "overclockAEffectZh",
    "overclockAObjectPath", "overclockBId", "overclockBNameZh", "overclockBNameEn",
    "overclockBEffectZh", "overclockBObjectPath", "orderedLeafObjectPaths",
    "finalLeafObjectPaths", "duplicateLeafObjectPaths", "mergedSharedLeaves",
    "numericStackDecisions",
    "sharedWriteTargets", "route",
    "feasibility", "implementationComplexity", "bridgeHandlerId", "rowHash",
  ];
  const rows = pairs.map((row) => {
    const flat = {
      ...row,
      Name: `Pair_${String(row.testIndex).padStart(3, "0")}`,
      orderedLeafObjectPaths: row.orderedLeafObjectPaths.join("|"),
      finalLeafObjectPaths: row.finalLeafObjectPaths.join("|"),
      duplicateLeafObjectPaths: row.duplicateLeafObjectPaths.join("|"),
      mergedSharedLeaves: JSON.stringify(row.mergedSharedLeaves),
      numericStackDecisions: JSON.stringify(row.numericStackDecisions),
      sharedWriteTargets: row.sharedWriteTargets.join("|"),
    };
    return columns.map((column) => csvCell(flat[column])).join(",");
  });
  return `${columns.join(",")}\n${rows.join("\n")}\n`;
}

function buildTestIndexCsv(pairs) {
  const columns = [
    "testIndex", "pairId", "职业", "槽位", "武器", "超频A", "超频B",
    "路线", "复杂度", "桥接处理器",
  ];
  const rows = pairs.map((row) => [
    row.testIndex, row.pairId, row.classNameZh, row.slot, row.weaponNameZh,
    row.overclockANameZh, row.overclockBNameZh, row.route,
    row.implementationComplexity, row.bridgeHandlerId,
  ].map(csvCell).join(","));
  return `${columns.join(",")}\n${rows.join("\n")}\n`;
}

export function buildRuntimeManifest(outputPaths = {}, publicPrepared = null) {
  if (publicPrepared) return buildPublicPreparedRuntime(outputPaths, publicPrepared);
  const runtimeManifestPath = outputPaths.runtimeManifest ?? files.runtimeManifest;
  const runtimePairsCsvPath = outputPaths.runtimePairsCsv ?? files.runtimePairsCsv;
  const runtimeTestIndexCsvPath = outputPaths.runtimeTestIndexCsv ?? files.runtimeTestIndexCsv;
  const catalog = readJson(files.catalog);
  const implementations = readJson(files.implementations);
  const pairAssessments = readJson(files.pairAssessments);
  const parsedIndex = readJson(files.parsedIndex);
  const numericPolicy = readJson(files.numericStackPolicy);
  const mirrorConfig = readJson(files.weaponMirrors);
  const overclocks = buildOverclockRows(catalog, implementations, parsedIndex);
  const leafMergeInfo = buildLeafMergeInfo(implementations);
  const legacyPairs = buildPairRows(
    pairAssessments,
    overclocks,
    leafMergeInfo,
    numericPolicy,
    mirrorConfig,
  );
  const gf02 = loadGF02Inputs();
  const gf04 = loadGF04Inputs();
  const gf03 = loadGF03Inputs();
  const remaining=loadRemainingInputs();
  const catalogPairs=appendRemainingCatalogRows(legacyPairs,remaining.contract,overclocks,pairAssessments);
  const pairs = overlayRemainingRecipes(overlayGF03Recipes(overlayGF04Recipes(overlayGF02Recipes(catalogPairs, gf02.contract, gf02.evidence), gf04.contract, gf04.evidence), gf03.contract, gf03.evidence),remaining.contract,remaining.evidence).map((row) => {
    const { rowHash, ...core } = row;
    if (core.testIndex === 259) {
      if (core.pairId !== "8a3b70ef101cffc8") throw new Error("RJ250 double recipe identity changed");
      core.rj250Double = {
        nativeBaseline: 900,
        componentClassPath: "/Game/DualOverclockReward/Components/GrenadeLauncher/BP_DOR_RJ250Double.BP_DOR_RJ250Double_C",
        leafObjectPath: "/Game/DualOverclockReward/AdjustedLeaves/GrenadeLauncher/DOR_RJ250Double.DOR_RJ250Double",
      };

    }
    return { ...core, rowHash: sha256(stableJson(core)) };
  });
  const source = {
    remainingContractSha256:remaining.contractSha256,
    remainingResolvedInputsSha256:remaining.evidenceSha256,
    gf03ContractSha256: gf03.contractSha256,
    gf03ResolvedInputsSha256: gf03.evidenceSha256,
    gf04ContractSha256: gf04.contractSha256,
    gf04ResolvedInputsSha256: gf04.evidenceSha256,
    gf02ContractSha256: gf02.contractSha256,
    gf02ResolvedInputsSha256: gf02.evidenceSha256,
    gameBuild: implementations.metadata?.gameBuild ?? null,
    implementationSchema: implementations.schema,
    pairAssessmentSchema: pairAssessments.schema,
  };
  const highLoadFilter = readJson(new URL("../../data/manual/high-load-policy.json", import.meta.url));
  if (highLoadFilter.schema !== "dor.high-load-policy.v1" || highLoadFilter.defaultEnabled !== false || highLoadFilter.exclusions.length !== 37) throw new Error("Invalid reviewed high-load policy");
  if (new Set(highLoadFilter.exclusions.map(row => row.pairId)).size !== 37 || new Set(highLoadFilter.exclusions.map(row => row.runtimeCode)).size !== 37) throw new Error("Duplicate high-load policy identity");
  for (const row of highLoadFilter.exclusions) {
    if (pairs.find(p => p.pairId === row.pairId)?.testIndex !== row.runtimeCode) throw new Error(`High-load policy identity mismatch: ${row.pairId}`);
  }
  const manifestCore = {
    schema: "dual-overclock-reward.runtime-manifest.v1",
    source,
    policy: {
      supportedComplexityMaxExclusive: SUPPORTED_COMPLEXITY_MAX_EXCLUSIVE,
      runtimeNativeDependency: false,
      noMechanicTextInference: true,
      leafOrder:
        "A-then-B with indexed numeric substitutions; preserve native integer per-leaf rounding; " +
        "known mixed ADD/MUL ordering follows utility; unconsumed original occurrences retain order",
      numericStacking:
        "preserve known native ADD/MUL; only unknown operators choose legal alternatives by utility; " +
        "mixed known ADD/MUL uses less-beneficial order for two benefits and more-beneficial order " +
        "for penalties or mixed contributions; dimensionful flats never multiply",
      choiceTimeoutSeconds: 30,
      highLoadFilter,
      debugAuthority: "listen-host-self-only",
      deepDiveEnabled: false,
    },
    metadata: {
      weaponCount: catalog.weapons.length,
      overclockCount: overclocks.length,
      supportedPairCount: pairs.length,
      numericMergePairCount: pairs.filter((row) => row.mergedSharedLeaves.length).length,
      numericMergeLeafCount: pairs.reduce((sum, row) => sum + row.mergedSharedLeaves.length, 0),
      gf02PairCount: gf02.contract.recipes.length,
      gf04PairCount: gf04.contract.recipes.length,
      gf03PairCount: gf03.contract.recipes.length,
      remainingPreparedPairCount:remaining.contract.recipes.length,
      numericAdjustedLeafCount: pairs.reduce((sum, row) => sum + row.adjustedLeaves.length, 0),
      excludedPairCount: pairAssessments.assessments.length - pairs.length,
    },
    overclocks,
    pairs,
  };
  const manifest = {
    ...manifestCore,
    manifestHash: sha256(stableJson(manifestCore)),
  };
  assertManifest(manifest);
  writeJson(runtimeManifestPath, manifest);
  writeText(runtimePairsCsvPath, buildRuntimeCsv(pairs));
  writeText(runtimeTestIndexCsvPath, buildTestIndexCsv(pairs));
  return manifest;
}

// Explicit public route: descriptors retain maintained mechanisms; recipe numeric
// fields are rebuilt through the same production overlays, never historical inputs.
function buildPublicPreparedRuntime(outputs, prepared) {
  const { base, overclocks, families, provenance } = prepared;
  const {gf02,gf03,gf04,remaining}=families;
  const pairs=overlayRemainingRecipes(overlayGF03Recipes(overlayGF04Recipes(overlayGF02Recipes(
    structuredClone(base.pairs),gf02.contract,gf02.evidence),gf04.contract,gf04.evidence),gf03.contract,gf03.evidence),remaining.contract,remaining.evidence)
    .map(({rowHash,...core})=>({...core,rowHash:sha256(stableJson(core))}));
  const core={schema:'dual-overclock-reward.runtime-manifest.v1',source:provenance,
    policy:base.policy,metadata:{...base.metadata},overclocks,pairs};
  const manifest={...core,manifestHash:sha256(stableJson(core))};
  assertManifest(manifest);
  writeJson(outputs.runtimeManifest??files.runtimeManifest,manifest);
  writeText(outputs.runtimePairsCsv??files.runtimePairsCsv,buildRuntimeCsv(pairs));
  writeText(outputs.runtimeTestIndexCsv??files.runtimeTestIndexCsv,buildTestIndexCsv(pairs));
  return manifest;
}

