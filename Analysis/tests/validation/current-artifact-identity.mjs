// Single intentional update point for the accepted current artifact identity.
export const ACCEPTED_BUILD_TAG = "0.93.14-alpha170-contributor-reviewed16-nonrelease";
export const EXPECTED_PACKAGE_FILENAME =
  `DualOverclockReward-${ACCEPTED_BUILD_TAG.slice(ACCEPTED_BUILD_TAG.indexOf("alpha"))}-final.pak`;

function capture(pattern, value, description) {
  const match = value.match(pattern);
  if (!match) throw new Error(`Missing ${description}`);
  return match[1];
}

export function readCurrentArtifactIdentity({ generatorSource, generationScript, packageScript }) {
  const buildTag = capture(
    /constexpr\s+TCHAR\s+DORResearchBuildTag\[\]\s*=\s*TEXT\("([^"]+)"\)/,
    generatorSource,
    "DORResearchBuildTag constexpr",
  );
  const expectedBuild = capture(
    /^\$expectedBuild\s*=\s*'([^']+)'/m,
    generationScript,
    "Generate expected build tag",
  );
  const packageFilename = capture(
    /^\$pakFile\s*=\s*Join-Path\s+\$builds\s+'([^']+)'/m,
    packageScript,
    "Package-DOR pak filename assignment",
  );
  if (buildTag !== ACCEPTED_BUILD_TAG) {
    throw new Error(`Unexpected current C++ build tag: ${buildTag}`);
  }
  if (expectedBuild !== buildTag) {
    throw new Error(`Generate expected build tag does not match C++ tag: ${expectedBuild}`);
  }
  if (packageFilename !== EXPECTED_PACKAGE_FILENAME) {
    throw new Error(`Package filename does not match current build identity: ${packageFilename}`);
  }
  return { buildTag, expectedBuild, packageFilename };
}
