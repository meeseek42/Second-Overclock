// Known consumer operators take precedence over utility-based alternatives.
export function selectNumeric(amounts, isPercentage, utility = null, nativeOperator = "UNKNOWN") {
  if (amounts.some(n => !Number.isFinite(n))) throw Error("Nonfinite numeric input");
  if (!["ADD", "MUL", "UNKNOWN"].includes(nativeOperator)) throw Error("Unknown native operator provenance");
  if (nativeOperator !== "UNKNOWN" || !isPercentage) {
    const multiply = nativeOperator === "MUL";
    const value = amounts.reduce((a, b) => multiply ? a * b : a + b, multiply ? 1 : 0);
    if (!Number.isFinite(value) || (multiply && value < 0)) throw Error("Illegal native numeric result");
    const mode = multiply ? "multiplicative" : "flat-additive";
    return {mode, value, legal: true, neutral: multiply ? 1 : 0, utility,
      candidates: [{mode, value, legal: true}], nativeOperator,
      rule: nativeOperator === "UNKNOWN" ? "only-unit-legal-candidate" : "preserve-known-native-operator"};
  }
  if (!["higher-is-better", "lower-is-better"].includes(utility)) throw Error("Missing utility");
  const polarities = amounts.map(n => n === 1 ? "neutral" : (n > 1) === (utility === "higher-is-better") ? "positive" : "negative");
  const favor = !polarities.every(p => p === "positive");
  const candidates = [{mode: "additive-deviation-from-neutral", value: 1 + amounts.reduce((s, n) => s + n - 1, 0)},
    {mode: "multiplicative", value: amounts.reduce((s, n) => s * n, 1)}].map(c => ({...c, legal: Number.isFinite(c.value) && c.value >= 0}));
  const legal = candidates.filter(c => c.legal);
  if (!legal.length) throw Error("No legal candidate");
  const higher = favor === (utility === "higher-is-better");
  const picked = legal.reduce((a, b) => (higher ? b.value > a.value : b.value < a.value) ? b : a);
  return {...picked, neutral: 1, utility, polarities, candidates, nativeOperator,
    rule: favor ? "player-favorable-negative-or-mixed" : "player-unfavorable-double-positive"};
}

export function validateNativeSelection(op) {
  if (!op.nativeOperatorProvenance) return;
  const operator = op.nativeOperatorProvenance.operator;
  const selected = selectNumeric(op.sourceOccurrences.map(s => s.amount.value), op.isPercentage ?? true, op.selection.utility, operator);
  if (op.selection.nativeOperator !== operator || op.selection.value !== selected.value ||
      (op.operation !== "ordered-native-integer-contributions" && op.selection.mode !== selected.mode)) throw Error("Known native operator selection mismatch");
}

// Reuse GF03's indexed placement: multiple outputs occupy the original numeric
// occurrence positions, without moving intervening mechanism leaves.
export function placeNumericOccurrences(ordered, operations, sourceEdits = []) {
  const consumed = new Set(), at = new Map();
  for (const op of operations) {
    const indices = (op.consumedSourceIndices ?? op.sourceOccurrences.map(s => s.index)).slice().sort((a, b) => a - b);
    if (!indices.length) continue;
    const outputs = op.outputLeafObjectPaths ?? [op.adjustedObjectPath ?? op.mergedObjectPath];
    if (outputs.length > indices.length) throw Error("Numeric placement overflow");
    for (const i of indices) {if (consumed.has(i)) throw Error("Double numeric consumption"); consumed.add(i);}
    outputs.forEach((p, i) => {
      const index = outputs.length === 1 ? indices[0] : i === outputs.length - 1 ? indices.at(-1) : indices[i];
      if (at.has(index)) throw Error("Numeric placement conflict");
      at.set(index, [p]);
    });
  }
  const removed = new Set(), after = new Map();
  for (const edit of sourceEdits) {
    const replaces = ['replace-with-generated-source', 'move-generated-source'].includes(edit.operation);
    const index = edit.operation === 'remove-original-source' || replaces ? edit.sourceIndex : edit.afterSourceIndex;
    if (!Number.isInteger(index) || index < 0 || index >= ordered.length) throw Error('Source edit index outside original occurrences');
    if (edit.operation === 'remove-original-source' || replaces) {
      if (ordered[index] !== edit.sourceObjectPath || consumed.has(index) || removed.has(index)) throw Error('Source removal conflicts with original/numeric occurrence');
      removed.add(index);
      if (replaces) {
        if (!edit.outputObjectPath) throw Error('Missing generated source output');
        if (edit.operation === 'replace-with-generated-source') at.set(index, [edit.outputObjectPath]);
        else {
          if (!Number.isInteger(edit.afterSourceIndex) || edit.afterSourceIndex < 0 || edit.afterSourceIndex >= ordered.length || after.has(edit.afterSourceIndex)) throw Error('Generated source placement conflict');
          after.set(edit.afterSourceIndex, [edit.outputObjectPath]);
        }
      }
    } else if (edit.operation === 'insert-original-source') {
      if (consumed.has(index) || after.has(index) || !edit.sourceObjectPath) throw Error('Source insertion conflicts with original/numeric occurrence');
      after.set(index, [edit.sourceObjectPath]);
    } else if (edit.operation === 'insert-generated-source') {
      if (ordered[index] !== edit.sourceObjectPath || consumed.has(index) || removed.has(index) || after.has(index) || !edit.outputObjectPath) throw Error('Generated insertion conflicts with original occurrence');
      after.set(index, [edit.outputObjectPath]);
    } else throw Error('Unsupported source edit');
  }
  return ordered.flatMap((p, i) => [...(at.get(i) ?? (consumed.has(i) || removed.has(i) ? [] : [p])), ...(after.get(i) ?? [])]);
}
