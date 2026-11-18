// Deliberate balance outputs preserve the computed native selection and inputs.
export function effectiveNumericResult(op) {
  if (op.outputOverride === undefined) return Math.fround(op.selection.value);
  const override = op.outputOverride;
  if (!['emit-native-numeric-leaf', 'emit-native-flat-adjustment'].includes(op.operation) ||
      !['Amount', 'Damage'].includes(op.valueField) || !op.sourceClassPath?.startsWith('/Script/FSD.') ||
      !['new-native-standard', 'duplicate-native-template'].includes(op.assetCreation)) throw Error('Override unsupported on preservation/integer/non-numeric operation');
  if (!override || Object.keys(override).sort().join(',') !== 'reason,value' ||
      typeof override.reason !== 'string' || !override.reason.trim() || override.reason.length > 500 ||
      typeof override.value !== 'number' || !Number.isFinite(override.value) || !Number.isFinite(Math.fround(override.value)) ||
      (op.isPercentage && override.value < 0)) throw Error('Invalid numeric output override');
  return Math.fround(override.value);
}

