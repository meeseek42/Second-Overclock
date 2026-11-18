using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using UAssetAPI;
using UAssetAPI.UnrealTypes;
using UAssetAPI.Unversioned;

if (args.Length < 2)
{
    Console.Error.WriteLine("Usage: UAssetInspector <input-list.txt> <output-directory> [mappings.usmap]");
    return 2;
}

var listPath = Path.GetFullPath(args[0]);
var outputDirectory = Path.GetFullPath(args[1]);
var mappingsPath = args.Length >= 3 ? Path.GetFullPath(args[2]) : null;
Directory.CreateDirectory(outputDirectory);

Usmap? mappings = null;
if (!string.IsNullOrWhiteSpace(mappingsPath) && File.Exists(mappingsPath))
{
    mappings = new Usmap(mappingsPath);
}

var inputs = File.ReadAllLines(listPath)
    .Select(line => line.Trim())
    .Where(line => line.Length > 0)
    .Distinct(StringComparer.OrdinalIgnoreCase)
    .OrderBy(line => line, StringComparer.OrdinalIgnoreCase)
    .ToArray();

var results = new List<object>();
foreach (var input in inputs)
{
    var fullPath = Path.GetFullPath(input);
    var id = Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(fullPath))).ToLowerInvariant()[..20];
    var output = Path.Combine(outputDirectory, $"{id}.json");
    try
    {
        var asset = new UAsset(fullPath, EngineVersion.VER_UE4_27, mappings);
        File.WriteAllText(output, asset.SerializeJson(true), new UTF8Encoding(false));
        results.Add(new
        {
            input = fullPath,
            success = true,
            output,
            error = (string?)null
        });
    }
    catch (Exception exception)
    {
        results.Add(new
        {
            input = fullPath,
            success = false,
            output = (string?)null,
            error = $"{exception.GetType().FullName}: {exception.Message}"
        });
    }
}

var resultPath = Path.Combine(outputDirectory, "batch-result.json");
File.WriteAllText(
    resultPath,
    JsonSerializer.Serialize(
        new
        {
            schema = "dual-overclock-reward.uasset-batch-result.v1",
            engine = "UE4.27",
            mappings = mappingsPath,
            results
        },
        new JsonSerializerOptions { WriteIndented = true }
    ),
    new UTF8Encoding(false)
);

Console.WriteLine(resultPath);
return results.Any(result => !(bool)(result.GetType().GetProperty("success")?.GetValue(result) ?? false)) ? 1 : 0;
