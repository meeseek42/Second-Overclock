using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using UAssetAPI;
using UAssetAPI.ExportTypes;
using UAssetAPI.PropertyTypes.Objects;
using UAssetAPI.PropertyTypes.Structs;
using UAssetAPI.UnrealTypes;

const string ToolSchema = "dual-overclock-reward.cooked-weapon-mirror-result.v1";

if (args.Length == 1 && args[0] == "reflect")
{
    foreach (var type in new[]
    {
        typeof(NormalExport), typeof(ObjectPropertyData), typeof(FloatPropertyData),
        typeof(BoolPropertyData), typeof(ArrayPropertyData), typeof(SoftObjectPropertyData),
        typeof(FSoftObjectPath), typeof(FTopLevelAssetPath), typeof(FPackageIndex)
    })
    {
        Console.WriteLine($"TYPE {type.FullName}");
        foreach (var constructor in type.GetConstructors())
        {
            Console.WriteLine($"CTOR {constructor}");
        }
        foreach (var property in type.GetProperties())
        {
            Console.WriteLine($"PROP {property.PropertyType.FullName} {property.Name} write={property.CanWrite}");
        }
        foreach (var field in type.GetFields())
        {
            Console.WriteLine($"FIELD {field.FieldType.FullName} {field.Name}");
        }
    }
    return 0;
}

if (args.Length == 2 && args[0] == "inspect")
{
    var inspected = new UAsset(Path.GetFullPath(args[1]), EngineVersion.VER_UE4_27);
    Console.WriteLine($"asset={Path.GetFullPath(args[1])}");
    Console.WriteLine($"exports={inspected.Exports.Count} imports={inspected.Imports.Count}");
    for (var exportIndex = 0; exportIndex < inspected.Exports.Count; exportIndex++)
    {
        var export = inspected.Exports[exportIndex];
        Console.WriteLine(
            $"export[{exportIndex}] name={export.ObjectName} class={ResolveIndex(inspected, export.ClassIndex)} " +
            $"outer={ResolveIndex(inspected, export.OuterIndex)} type={export.GetType().Name}");
        if (export is NormalExport normal)
        {
            foreach (var property in normal.Data)
            {
                PrintProperty(inspected, property, "  ");
            }
        }
    }
    return 0;
}

if (args.Length == 11 && args[0] == "override-oc")
{
    var overrideOptions = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
    for (var index = 1; index + 1 < args.Length; index += 2)
    {
        overrideOptions[args[index]] = args[index + 1];
    }
    string OverrideOption(string name) => overrideOptions.TryGetValue(name, out var value)
        ? Path.GetFullPath(value)
        : throw new ArgumentException($"Missing required option {name}.");

    var overrideConfigPath = OverrideOption("--config");
    var nativeOcPath = OverrideOption("--source-native");
    var fusionOcPath = OverrideOption("--source-fusion");
    var overrideOutputPath = OverrideOption("--output");
    var overrideResultPath = OverrideOption("--result");
    var overrideConfig = JsonSerializer.Deserialize<NativeOcOverrideConfiguration>(
        File.ReadAllText(overrideConfigPath),
        new JsonSerializerOptions { PropertyNameCaseInsensitive = true })
        ?? throw new InvalidDataException("Native OC override configuration is empty.");
    BuildNativeOcOverride(
        overrideConfig,
        nativeOcPath,
        fusionOcPath,
        overrideOutputPath,
        overrideResultPath);
    Console.WriteLine(overrideResultPath);
    return 0;
}

if (args.Length != 13 || args[0] != "mirror")
{
    Console.Error.WriteLine(
        "Usage: CookedWeaponMirror mirror --config <json> --source-weapon <uasset> " +
        "--source-bank <uasset> --source-item-id <uasset> " +
        "--cooked-content <directory> --result <json>");
    return 2;
}

var options = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
for (var index = 1; index + 1 < args.Length; index += 2)
{
    options[args[index]] = args[index + 1];
}

var configPath = RequiredOption("--config");
var sourceWeaponPath = RequiredOption("--source-weapon");
var sourceBankPath = RequiredOption("--source-bank");
var sourceItemIdPath = RequiredOption("--source-item-id");
var cookedContent = RequiredOption("--cooked-content");
var resultPath = RequiredOption("--result");

var config = JsonSerializer.Deserialize<MirrorConfiguration>(
    File.ReadAllText(configPath),
    new JsonSerializerOptions { PropertyNameCaseInsensitive = true })
    ?? throw new InvalidDataException("Mirror configuration is empty.");
if (config.VerticalOnly != true || config.Weapons is null || config.Weapons.Count != 1)
{
    throw new InvalidDataException(
        "The vertical build must contain exactly one weapon and verticalOnly=true.");
}

var weapon = config.Weapons[0];
var sourceWeaponUexp = Path.ChangeExtension(sourceWeaponPath, ".uexp");
var sourceBankUexp = Path.ChangeExtension(sourceBankPath, ".uexp");
var sourceItemIdUexp = Path.ChangeExtension(sourceItemIdPath, ".uexp");
RequireHash(sourceWeaponPath, weapon.SourceWeaponUassetSha256, "source weapon uasset");
RequireHash(sourceWeaponUexp, weapon.SourceWeaponUexpSha256, "source weapon uexp");
RequireHash(sourceBankPath, weapon.SourceBankUassetSha256, "source bank uasset");
RequireHash(sourceBankUexp, weapon.SourceBankUexpSha256, "source bank uexp");
RequireHash(sourceItemIdPath, weapon.SourceItemIdUassetSha256, "source ItemID uasset");
RequireHash(sourceItemIdUexp, weapon.SourceItemIdUexpSha256, "source ItemID uexp");

var mirrorWeaponPath = CookedPath(cookedContent, weapon.MirrorWeaponPackage);
var mirrorBankPath = CookedPath(cookedContent, weapon.MirrorBankPackage);
var mirrorItemIdPath = CookedPath(cookedContent, weapon.MirrorItemIdPackage);
Directory.CreateDirectory(Path.GetDirectoryName(mirrorWeaponPath)!);
Directory.CreateDirectory(Path.GetDirectoryName(mirrorBankPath)!);
Directory.CreateDirectory(Path.GetDirectoryName(mirrorItemIdPath)!);

Console.Error.WriteLine("[mirror] loading source weapon");
var weaponAsset = new UAsset(sourceWeaponPath, EngineVersion.VER_UE4_27);
Console.Error.WriteLine("[mirror] patching source weapon");
var originalWeaponExportCount = weaponAsset.Exports.Count;
RenameExact(
    weaponAsset,
    new Dictionary<string, string>(StringComparer.Ordinal)
    {
        [weapon.SourceWeaponPackage] = weapon.MirrorWeaponPackage,
        [$"{weapon.SourceWeaponObject}_C"] = $"{weapon.MirrorWeaponObject}_C",
        [$"Default__{weapon.SourceWeaponObject}_C"] = $"Default__{weapon.MirrorWeaponObject}_C",
        [$"ExecuteUbergraph_{weapon.SourceWeaponObject}"] = $"ExecuteUbergraph_{weapon.MirrorWeaponObject}",
        [weapon.SourceBankPackage] = weapon.MirrorBankPackage,
        [weapon.SourceBankObject] = weapon.MirrorBankObject
    });
    SetWeaponSavegameId(weaponAsset, weapon);
    var runtimeWeaponItemIdPath = string.Equals(
        weapon.WeaponItemIdMode,
        "native",
        StringComparison.OrdinalIgnoreCase)
        ? weapon.NativeItemIdPath
        : weapon.MirrorItemIdPath;
    SetWeaponItemId(weaponAsset, runtimeWeaponItemIdPath);
    ValidateWeaponItemId(weaponAsset, runtimeWeaponItemIdPath);
ValidateWeaponBankReference(weaponAsset, weapon.MirrorBankPackage, weapon.MirrorBankObject);
Console.Error.WriteLine("[mirror] writing mirror weapon");
weaponAsset.Write(mirrorWeaponPath);

Console.Error.WriteLine("[mirror] loading source ItemID");
var itemIdAsset = new UAsset(sourceItemIdPath, EngineVersion.VER_UE4_27);
var originalItemIdExportCount = itemIdAsset.Exports.Count;
RenameExact(
    itemIdAsset,
    new Dictionary<string, string>(StringComparer.Ordinal)
    {
        [weapon.SourceItemIdPackage] = weapon.MirrorItemIdPackage,
        [weapon.SourceItemIdObject] = weapon.MirrorItemIdObject,
        [$"{weapon.SourceWeaponPackage}.{weapon.SourceWeaponObject}_C"] =
            $"{weapon.MirrorWeaponPackage}.{weapon.MirrorWeaponObject}_C"
    });
SetItemIdSavegameId(itemIdAsset, weapon);
ValidateItemIdWeaponReference(itemIdAsset, weapon);
Console.Error.WriteLine("[mirror] writing mirror ItemID");
itemIdAsset.Write(mirrorItemIdPath);

Console.Error.WriteLine("[mirror] loading source bank");
var bankAsset = new UAsset(sourceBankPath, EngineVersion.VER_UE4_27);
var originalBankExportCount = bankAsset.Exports.Count;
RenameExact(
    bankAsset,
    new Dictionary<string, string>(StringComparer.Ordinal)
    {
        [weapon.SourceBankPackage] = weapon.MirrorBankPackage,
        [weapon.SourceBankObject] = weapon.MirrorBankObject
    });
AppendFusionRoot(bankAsset, weapon);
Console.Error.WriteLine("[mirror] writing mirror bank");
bankAsset.Write(mirrorBankPath);

Console.Error.WriteLine("[mirror] reloading mirrors");
var reloadedWeapon = new UAsset(mirrorWeaponPath, EngineVersion.VER_UE4_27);
var reloadedBank = new UAsset(mirrorBankPath, EngineVersion.VER_UE4_27);
var reloadedItemId = new UAsset(mirrorItemIdPath, EngineVersion.VER_UE4_27);
ValidateMirror(
    reloadedWeapon,
    reloadedBank,
    reloadedItemId,
    weapon,
    originalWeaponExportCount,
    originalBankExportCount,
    originalItemIdExportCount);

var result = new
{
    schema = ToolSchema,
    gameBuild = config.GameBuild,
    verticalOnly = true,
    testIndex = weapon.TestIndex,
    pairId = weapon.PairId,
    source = new
    {
        weaponUassetSha256 = Sha256(sourceWeaponPath),
        weaponUexpSha256 = Sha256(sourceWeaponUexp),
        bankUassetSha256 = Sha256(sourceBankPath),
        bankUexpSha256 = Sha256(sourceBankUexp),
        itemIdUassetSha256 = Sha256(sourceItemIdPath),
        itemIdUexpSha256 = Sha256(sourceItemIdUexp)
    },
    mirror = new
    {
        weaponPackage = weapon.MirrorWeaponPackage,
        weaponClassPath = $"{weapon.MirrorWeaponPackage}.{weapon.MirrorWeaponObject}_C",
        weaponSavegameId = weapon.MirrorSavegameId,
        bankPackage = weapon.MirrorBankPackage,
        fusionRootPath = weapon.FusionRootPath,
        mirrorItemIdPath = weapon.MirrorItemIdPath,
        nativeItemIdPath = weapon.NativeItemIdPath,
        weaponUassetSha256 = Sha256(mirrorWeaponPath),
        weaponUexpSha256 = Sha256(Path.ChangeExtension(mirrorWeaponPath, ".uexp")),
        bankUassetSha256 = Sha256(mirrorBankPath),
        bankUexpSha256 = Sha256(Path.ChangeExtension(mirrorBankPath, ".uexp")),
        itemIdUassetSha256 = Sha256(mirrorItemIdPath),
        itemIdUexpSha256 = Sha256(Path.ChangeExtension(mirrorItemIdPath, ".uexp")),
        weaponExportCount = reloadedWeapon.Exports.Count,
        bankExportCount = reloadedBank.Exports.Count,
        itemIdExportCount = reloadedItemId.Exports.Count,
        bankOverclockCount = GetOverclockMap(reloadedBank, weapon.MirrorBankObject).Value.Count
    }
};
Directory.CreateDirectory(Path.GetDirectoryName(resultPath)!);
File.WriteAllText(
    resultPath,
    JsonSerializer.Serialize(result, new JsonSerializerOptions { WriteIndented = true }),
    new UTF8Encoding(false));
Console.WriteLine(resultPath);
return 0;

string RequiredOption(string name)
{
    if (!options.TryGetValue(name, out var value) || string.IsNullOrWhiteSpace(value))
    {
        throw new ArgumentException($"Missing required option {name}.");
    }
    return Path.GetFullPath(value);
}

static string CookedPath(string cookedContent, string packagePath)
{
    const string gamePrefix = "/Game/";
    if (!packagePath.StartsWith(gamePrefix, StringComparison.Ordinal))
    {
        throw new InvalidDataException($"Not a /Game package: {packagePath}");
    }
    var relative = packagePath[gamePrefix.Length..].Replace('/', Path.DirectorySeparatorChar);
    return Path.Combine(Path.GetFullPath(cookedContent), relative + ".uasset");
}

static void RenameExact(UAsset asset, IReadOnlyDictionary<string, string> replacements)
{
    var names = asset.GetNameMapIndexList().Select(value => value.ToString()).ToArray();
    foreach (var (from, to) in replacements)
    {
        var replaced = 0;
        for (var index = 0; index < names.Length; index++)
        {
            if (!string.Equals(names[index], from, StringComparison.Ordinal))
            {
                continue;
            }
            asset.SetNameReference(index, new FString(to));
            names[index] = to;
            replaced++;
        }
        if (replaced == 0)
        {
            throw new InvalidDataException($"Expected exact name was not found: {from}");
        }
    }
}

static void SetWeaponSavegameId(UAsset asset, MirrorWeapon weapon)
{
    var cdoName = $"Default__{weapon.MirrorWeaponObject}_C";
    var cdo = asset.Exports.OfType<NormalExport>()
        .Single(export => export.ObjectName.ToString() == cdoName);
    var savegame = cdo.Data.OfType<StructPropertyData>()
        .Single(property => property.Name.ToString() == "SavegameID");
    var guid = savegame.Value.OfType<GuidPropertyData>().Single();
    guid.Value = Guid.Parse(weapon.MirrorSavegameId);
}

static void SetItemIdSavegameId(UAsset asset, MirrorWeapon weapon)
{
    var itemId = asset.Exports.OfType<NormalExport>()
        .Single(export => export.ObjectName.ToString() == weapon.MirrorItemIdObject);
    var savegame = itemId.Data.OfType<StructPropertyData>()
        .Single(property => string.Equals(
            property.Name.ToString(),
            "SaveGameID",
            StringComparison.OrdinalIgnoreCase));
    savegame.Value.OfType<GuidPropertyData>().Single().Value =
        Guid.Parse(weapon.MirrorSavegameId);
}

static void ValidateItemIdWeaponReference(UAsset asset, MirrorWeapon weapon)
{
    var itemId = asset.Exports.OfType<NormalExport>()
        .Single(export => export.ObjectName.ToString() == weapon.MirrorItemIdObject);
    var item = itemId.Data.OfType<SoftObjectPropertyData>()
        .Single(property => property.Name.ToString() == "Item");
    var actual = item.Value.AssetPath.AssetName.ToString();
    var expected = $"{weapon.MirrorWeaponPackage}.{weapon.MirrorWeaponObject}_C";
    if (!string.Equals(actual, expected, StringComparison.Ordinal))
    {
        throw new InvalidDataException(
            $"Mirror ItemID weapon mismatch: expected {expected}, got {actual}.");
    }
}

static void SetWeaponItemId(UAsset asset, string itemIdPath)
{
    var cdo = asset.Exports.OfType<NormalExport>()
        .Single(export => export.ObjectName.ToString().StartsWith("Default__WPN_DOR_", StringComparison.Ordinal));
    var itemId = cdo.Data.OfType<ObjectPropertyData>()
        .Single(property => property.Name.ToString() == "ItemID");
    var runtimeItemId = AddObjectImport(asset, itemIdPath, "/Script/FSD", "ItemID");
    itemId.Value = runtimeItemId;
    cdo.CreateBeforeSerializationDependencies.Add(runtimeItemId);
}

static void ValidateWeaponItemId(UAsset asset, string expectedPath)
{
    var cdo = asset.Exports.OfType<NormalExport>()
        .Single(export => export.ObjectName.ToString().StartsWith("Default__WPN_DOR_", StringComparison.Ordinal));
    var itemId = cdo.Data.OfType<ObjectPropertyData>()
        .Single(property => property.Name.ToString() == "ItemID");
    var resolved = ResolveIndex(asset, itemId.Value);
    if (!string.Equals(resolved, expectedPath, StringComparison.Ordinal))
    {
        throw new InvalidDataException($"Mirror ItemID mismatch: expected {expectedPath}, got {resolved}.");
    }
}

static void ValidateWeaponBankReference(UAsset asset, string expectedPackage, string expectedObject)
{
    var upgradable = asset.Exports.OfType<NormalExport>()
        .Single(export => export.Data.OfType<ObjectPropertyData>()
            .Any(property => property.Name.ToString() == "OverclockBank"));
    var bank = upgradable.Data.OfType<ObjectPropertyData>()
        .Single(property => property.Name.ToString() == "OverclockBank");
    var expected = $"{expectedPackage}.{expectedObject}";
    var resolved = ResolveIndex(asset, bank.Value);
    if (!string.Equals(resolved, expected, StringComparison.Ordinal))
    {
        throw new InvalidDataException($"Mirror bank mismatch: expected {expected}, got {resolved}.");
    }
}

static void AppendFusionRoot(UAsset bank, MirrorWeapon weapon)
{
    var pair = AddObjectImport(bank, weapon.FusionRootPath, "/Script/FSD", "CombinedUpgrade");
    var schematic = AddObjectImport(bank, weapon.FusionSchematicPath, "/Script/FSD", "Schematic");
    var map = GetOverclockMap(bank, weapon.MirrorBankObject);
    if (map.Value.Any(entry => ResolveIndex(bank, ((ObjectPropertyData)entry.Key).Value) == weapon.FusionRootPath))
    {
        throw new InvalidDataException("Fusion root already exists in source bank.");
    }
    map.Value.Add(
        new ObjectPropertyData(new FName(bank, "Overclocks")) { Value = pair },
        new ObjectPropertyData(new FName(bank, "Overclocks")) { Value = schematic });

    var bankExport = bank.Exports.OfType<NormalExport>()
        .Single(export => export.ObjectName.ToString() == weapon.MirrorBankObject);
    bankExport.CreateBeforeSerializationDependencies.Add(pair);
    bankExport.CreateBeforeSerializationDependencies.Add(schematic);
}

static FPackageIndex AddObjectImport(
    UAsset asset,
    string fullObjectPath,
    string classPackage,
    string className)
{
    var dot = fullObjectPath.LastIndexOf('.');
    if (dot <= 0 || dot == fullObjectPath.Length - 1)
    {
        throw new InvalidDataException($"Object path must contain package and object: {fullObjectPath}");
    }
    var packagePath = fullObjectPath[..dot];
    var objectName = fullObjectPath[(dot + 1)..];
    var package = asset.AddImport(
        new Import("/Script/CoreUObject", "Package", new FPackageIndex(0), packagePath, false, asset));
    return asset.AddImport(
        new Import(classPackage, className, package, objectName, false, asset));
}

static MapPropertyData GetOverclockMap(UAsset asset, string bankObject)
{
    var export = asset.Exports.OfType<NormalExport>()
        .Single(item => item.ObjectName.ToString() == bankObject);
    return export.Data.OfType<MapPropertyData>()
        .Single(property => property.Name.ToString() == "Overclocks");
}

static string ResolveIndex(UAsset asset, FPackageIndex index)
{
    if (index.Index == 0)
    {
        return "None";
    }
    if (index.Index > 0)
    {
        var export = asset.Exports[index.Index - 1];
        var outer = ResolveIndex(asset, export.OuterIndex);
        return outer == "None" ? export.ObjectName.ToString() : $"{outer}.{export.ObjectName}";
    }
    var import = asset.Imports[-index.Index - 1];
    var importOuter = ResolveIndex(asset, import.OuterIndex);
    if (import.ClassName.ToString() == "Package")
    {
        return import.ObjectName.ToString();
    }
    return importOuter == "None"
        ? import.ObjectName.ToString()
        : $"{importOuter}.{import.ObjectName}";
}

static void PrintProperty(UAsset asset, PropertyData property, string indent)
{
    var prefix = $"{indent}{property.Name}:{property.GetType().Name}";
    switch (property)
    {
        case ObjectPropertyData objectProperty:
            Console.WriteLine($"{prefix}={ResolveIndex(asset, objectProperty.Value)}");
            break;
        case SoftObjectPropertyData softObjectProperty:
            Console.WriteLine($"{prefix}={softObjectProperty.Value.AssetPath.AssetName}");
            break;
        case ArrayPropertyData arrayProperty:
            Console.WriteLine($"{prefix} count={arrayProperty.Value.Count()}");
            foreach (var value in arrayProperty.Value)
            {
                PrintProperty(asset, value, indent + "  ");
            }
            break;
        case StructPropertyData structProperty:
            Console.WriteLine($"{prefix} struct={structProperty.StructType} count={structProperty.Value.Count}");
            foreach (var value in structProperty.Value)
            {
                PrintProperty(asset, value, indent + "  ");
            }
            break;
        default:
            Console.WriteLine($"{prefix}={property}");
            break;
    }
}

static void BuildNativeOcOverride(
    NativeOcOverrideConfiguration config,
    string nativeOcPath,
    string fusionOcPath,
    string outputPath,
    string resultPath)
{
    if (!config.VerticalOnly || config.TestIndex != 388)
    {
        throw new InvalidDataException("The construction-time override is restricted to vertical test 388.");
    }
    var nativeUexp = Path.ChangeExtension(nativeOcPath, ".uexp");
    var fusionUexp = Path.ChangeExtension(fusionOcPath, ".uexp");
    RequireHash(nativeOcPath, config.SourceNativeUassetSha256, "native OC uasset");
    RequireHash(nativeUexp, config.SourceNativeUexpSha256, "native OC uexp");
    if (!string.IsNullOrWhiteSpace(config.SourceFusionUassetSha256))
    {
        RequireHash(fusionOcPath, config.SourceFusionUassetSha256, "fusion OC uasset");
    }
    if (!string.IsNullOrWhiteSpace(config.SourceFusionUexpSha256))
    {
        RequireHash(fusionUexp, config.SourceFusionUexpSha256, "fusion OC uexp");
    }

    var native = new UAsset(nativeOcPath, EngineVersion.VER_UE4_27);
    var fusion = new UAsset(fusionOcPath, EngineVersion.VER_UE4_27);
    var nativeExport = native.Exports.OfType<NormalExport>()
        .Single(export => export.ObjectName.ToString() == config.NativeObject);
    var fusionExport = fusion.Exports.OfType<NormalExport>()
        .Single(export => export.ObjectName.ToString() == config.FusionObject);
    var nativeCombined = nativeExport.Data.OfType<ArrayPropertyData>()
        .Single(property => property.Name.ToString() == "CombinedUpgrades");
    var fusionCombined = fusionExport.Data.OfType<ArrayPropertyData>()
        .Single(property => property.Name.ToString() == "CombinedUpgrades");
    var nativeGuid = GetSaveGameGuid(nativeExport);
    if (nativeGuid != Guid.Parse(config.NativeSavegameId))
    {
        throw new InvalidDataException(
            $"Native OC SaveGameID mismatch: expected {config.NativeSavegameId}, got {nativeGuid}.");
    }

    var fusionLeaves = fusionCombined.Value.OfType<SoftObjectPropertyData>()
        .Select(property => SoftObjectPath(property.Value))
        .ToArray();
    if (!fusionLeaves.SequenceEqual(config.ExpectedFusionLeaves, StringComparer.Ordinal))
    {
        throw new InvalidDataException(
            "Fusion leaf order no longer matches the reviewed test388 contract. Actual: " +
            JsonSerializer.Serialize(fusionLeaves));
    }

    nativeCombined.Value = fusionCombined.Value.OfType<SoftObjectPropertyData>()
        .Select((property, index) => CloneSoftObjectProperty(native, property, index))
        .Cast<PropertyData>()
        .ToArray();

    Directory.CreateDirectory(Path.GetDirectoryName(outputPath)!);
    native.Write(outputPath);
    var reloaded = new UAsset(outputPath, EngineVersion.VER_UE4_27);
    var reloadedExport = reloaded.Exports.OfType<NormalExport>()
        .Single(export => export.ObjectName.ToString() == config.NativeObject);
    var reloadedLeaves = reloadedExport.Data.OfType<ArrayPropertyData>()
        .Single(property => property.Name.ToString() == "CombinedUpgrades")
        .Value.OfType<SoftObjectPropertyData>()
        .Select(property => SoftObjectPath(property.Value))
        .ToArray();
    if (!reloadedLeaves.SequenceEqual(config.ExpectedFusionLeaves, StringComparer.Ordinal))
    {
        throw new InvalidDataException("Reloaded native OC override did not preserve the fusion leaf order.");
    }
    if (GetSaveGameGuid(reloadedExport) != nativeGuid)
    {
        throw new InvalidDataException("Native OC SaveGameID changed while applying the construction-time override.");
    }
    if (reloaded.Exports.Count != native.Exports.Count)
    {
        throw new InvalidDataException("Native OC export count changed while applying the override.");
    }

    var result = new
    {
        schema = "dual-overclock-reward.native-oc-override-result.v1",
        config.GameBuild,
        config.TestIndex,
        config.NativePackage,
        config.NativeObject,
        nativeSavegameId = nativeGuid,
        originalLeafCount = config.OriginalLeafCount,
        fusionLeafCount = reloadedLeaves.Length,
        fusionLeaves = reloadedLeaves,
        sourceNativeUassetSha256 = Sha256(nativeOcPath),
        sourceNativeUexpSha256 = Sha256(nativeUexp),
        sourceFusionUassetSha256 = Sha256(fusionOcPath),
        sourceFusionUexpSha256 = Sha256(fusionUexp),
        outputUassetSha256 = Sha256(outputPath),
        outputUexpSha256 = Sha256(Path.ChangeExtension(outputPath, ".uexp"))
    };
    Directory.CreateDirectory(Path.GetDirectoryName(resultPath)!);
    File.WriteAllText(
        resultPath,
        JsonSerializer.Serialize(result, new JsonSerializerOptions { WriteIndented = true }),
        new UTF8Encoding(false));
}

static SoftObjectPropertyData CloneSoftObjectProperty(
    UAsset target,
    SoftObjectPropertyData source,
    int index)
{
    var packageName = source.Value.AssetPath.PackageName is null
        ? null
        : new FName(target, source.Value.AssetPath.PackageName.ToString());
    var assetName = new FName(target, source.Value.AssetPath.AssetName?.ToString() ?? "");
    return new SoftObjectPropertyData(new FName(target, index.ToString()))
    {
        Value = new FSoftObjectPath(
            new FTopLevelAssetPath(packageName!, assetName),
            new FString(source.Value.SubPathString?.ToString() ?? ""))
    };
}

static string SoftObjectPath(FSoftObjectPath value)
{
    var subPath = value.SubPathString?.ToString() ?? "";
    var packageName = value.AssetPath.PackageName?.ToString() ?? "";
    var assetName = value.AssetPath.AssetName?.ToString() ?? "";
    var objectPath = string.IsNullOrEmpty(packageName)
        ? assetName
        : $"{packageName}.{assetName}";
    return objectPath +
        (string.IsNullOrEmpty(subPath) ? "" : $":{subPath}");
}

static Guid GetSaveGameGuid(NormalExport export) => export.Data.OfType<StructPropertyData>()
    .Single(property => string.Equals(property.Name.ToString(), "SaveGameID", StringComparison.OrdinalIgnoreCase))
    .Value.OfType<GuidPropertyData>().Single().Value;

static void ValidateMirror(
    UAsset weaponAsset,
    UAsset bankAsset,
    UAsset itemIdAsset,
    MirrorWeapon weapon,
    int expectedWeaponExports,
    int expectedBankExports,
    int expectedItemIdExports)
{
    if (weaponAsset.Exports.Count != expectedWeaponExports)
    {
        throw new InvalidDataException("Weapon export count changed during mirroring.");
    }
    if (bankAsset.Exports.Count != expectedBankExports)
    {
        throw new InvalidDataException("Bank export count changed during mirroring.");
    }
    if (itemIdAsset.Exports.Count != expectedItemIdExports)
    {
        throw new InvalidDataException("ItemID export count changed during mirroring.");
    }
    _ = weaponAsset.Exports.Single(export => export.ObjectName.ToString() == $"{weapon.MirrorWeaponObject}_C");
    var expectedRuntimeItemId = string.Equals(
        weapon.WeaponItemIdMode,
        "native",
        StringComparison.OrdinalIgnoreCase)
        ? weapon.NativeItemIdPath
        : weapon.MirrorItemIdPath;
    ValidateWeaponItemId(weaponAsset, expectedRuntimeItemId);
    ValidateWeaponBankReference(weaponAsset, weapon.MirrorBankPackage, weapon.MirrorBankObject);
    ValidateItemIdWeaponReference(itemIdAsset, weapon);
    var mirrorItemId = itemIdAsset.Exports.OfType<NormalExport>()
        .Single(export => export.ObjectName.ToString() == weapon.MirrorItemIdObject);
    var actualItemIdGuid = mirrorItemId.Data.OfType<StructPropertyData>()
        .Single(property => string.Equals(
            property.Name.ToString(),
            "SaveGameID",
            StringComparison.OrdinalIgnoreCase))
        .Value.OfType<GuidPropertyData>().Single().Value;
    if (actualItemIdGuid != Guid.Parse(weapon.MirrorSavegameId))
    {
        throw new InvalidDataException("Mirror ItemID SaveGameID was not preserved after write/reload.");
    }

    var cdo = weaponAsset.Exports.OfType<NormalExport>()
        .Single(export => export.ObjectName.ToString() == $"Default__{weapon.MirrorWeaponObject}_C");
    var actualGuid = cdo.Data.OfType<StructPropertyData>()
        .Single(property => property.Name.ToString() == "SavegameID")
        .Value.OfType<GuidPropertyData>().Single().Value;
    if (actualGuid != Guid.Parse(weapon.MirrorSavegameId))
    {
        throw new InvalidDataException("Mirror SavegameID was not preserved after write/reload.");
    }

    var map = GetOverclockMap(bankAsset, weapon.MirrorBankObject);
    var pairEntry = map.Value.Single(entry =>
        ResolveIndex(bankAsset, ((ObjectPropertyData)entry.Key).Value) == weapon.FusionRootPath);
    var schematic = ResolveIndex(bankAsset, ((ObjectPropertyData)pairEntry.Value).Value);
    if (schematic != weapon.FusionSchematicPath)
    {
        throw new InvalidDataException($"Fusion schematic mismatch: {schematic}");
    }
}

static void RequireHash(string path, string expected, string label)
{
    if (!File.Exists(path))
    {
        throw new FileNotFoundException($"Missing {label}.", path);
    }
    var actual = Sha256(path);
    if (!string.Equals(actual, expected, StringComparison.OrdinalIgnoreCase))
    {
        throw new InvalidDataException(
            $"{label} hash changed. Expected {expected}, got {actual}. Re-audit the current game build.");
    }
}

static string Sha256(string path) =>
    Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(path))).ToLowerInvariant();

sealed class MirrorConfiguration
{
    public string GameBuild { get; set; } = "";
    public bool VerticalOnly { get; set; }
    public List<MirrorWeapon> Weapons { get; set; } = [];
}

sealed class NativeOcOverrideConfiguration
{
    public string GameBuild { get; set; } = "";
    public bool VerticalOnly { get; set; }
    public int TestIndex { get; set; }
    public string NativePackage { get; set; } = "";
    public string NativeObject { get; set; } = "";
    public string NativeSavegameId { get; set; } = "";
    public string FusionObject { get; set; } = "";
    public int OriginalLeafCount { get; set; }
    public string SourceNativeUassetSha256 { get; set; } = "";
    public string SourceNativeUexpSha256 { get; set; } = "";
    public string SourceFusionUassetSha256 { get; set; } = "";
    public string SourceFusionUexpSha256 { get; set; } = "";
    public string[] ExpectedFusionLeaves { get; set; } = [];
}

sealed class MirrorWeapon
{
    public string Weapon { get; set; } = "";
    public int TestIndex { get; set; }
    public string PairId { get; set; } = "";
    public string SourceWeaponPackage { get; set; } = "";
    public string SourceWeaponObject { get; set; } = "";
    public string SourceWeaponUassetSha256 { get; set; } = "";
    public string SourceWeaponUexpSha256 { get; set; } = "";
    public string SourceBankPackage { get; set; } = "";
    public string SourceBankObject { get; set; } = "";
    public string SourceBankUassetSha256 { get; set; } = "";
    public string SourceBankUexpSha256 { get; set; } = "";
    public string SourceItemIdPackage { get; set; } = "";
    public string SourceItemIdObject { get; set; } = "";
    public string SourceItemIdUassetSha256 { get; set; } = "";
    public string SourceItemIdUexpSha256 { get; set; } = "";
    public string MirrorWeaponPackage { get; set; } = "";
    public string MirrorWeaponObject { get; set; } = "";
    public string MirrorBankPackage { get; set; } = "";
    public string MirrorBankObject { get; set; } = "";
    public string MirrorSavegameId { get; set; } = "";
    public string MirrorItemIdPackage { get; set; } = "";
    public string MirrorItemIdObject { get; set; } = "";
    public string MirrorItemIdPath { get; set; } = "";
    public string NativeItemIdPath { get; set; } = "";
    public string WeaponItemIdMode { get; set; } = "mirror";
    public string FusionRootPath { get; set; } = "";
    public string FusionSchematicPath { get; set; } = "";
}
