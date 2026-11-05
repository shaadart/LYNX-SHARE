#include "engine/ArchiveSafety.h"

#include <iostream>
#include <vector>

using Bytes = std::vector<std::uint8_t>;

Bytes archive (const std::vector<juce::String>& names, const std::string& text = "{}")
{
    juce::ZipFile::Builder builder;
    for (const auto& name : names)
        builder.addEntry (std::make_unique<juce::MemoryInputStream> (text.data(), text.size(), true), 9, name, juce::Time());
    juce::MemoryOutputStream out;
    if (! builder.writeToStream (out, nullptr))
        throw std::runtime_error ("could not build test archive");
    const auto* data = (const std::uint8_t*) out.getData();
    return { data, data + out.getDataSize() };
}

void changeCentralField (Bytes& bytes, int offset, std::uint32_t value)
{
    for (std::size_t i = 0; i + 46 <= bytes.size(); ++i)
        if (bytes[i] == 'P' && bytes[i + 1] == 'K' && bytes[i + 2] == 1 && bytes[i + 3] == 2)
            for (int b = 0; b < 4; ++b)
                bytes[i + (std::size_t) offset + b] = (std::uint8_t) (value >> (8 * b));
}

std::unique_ptr<juce::ZipFile> open (const Bytes& bytes)
{
    return std::make_unique<juce::ZipFile> (std::make_unique<juce::MemoryInputStream> (bytes.data(), bytes.size(), true));
}

int main()
{
    using namespace lynx::archivesafety;
    int failures = 0;
    auto check = [&] (bool ok, const char* what) {
        std::cout << (ok ? "PASS " : "FAIL ") << what << '\n';
        failures += ! ok;
    };
    const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                          .getNonexistentChildFile ("lynx-archive-security", {}, false);
    root.createDirectory();
    const juce::ScopeGuard cleanup { [&] { root.deleteRecursively(); } };
    juce::String json;

    auto zip = open (archive ({ "manifest.json", "project.lynx" }));
    check (validate (*zip).wasOk() && readManifest (*zip, json).wasOk() && json == "{}", "valid manifest");
    check (extract (*zip, root.getChildFile ("valid")).wasOk()
               && root.getChildFile ("valid/project.lynx").loadFileAsString() == "{}", "valid extraction");

    zip = open (archive ({ "project.lynx" }));
    check (readManifest (*zip, json).failed(), "missing manifest");
    zip = open (archive ({}));
    check (validate (*zip).failed(), "empty archive");

    auto bytes = archive ({ "manifest.json" });
    changeCentralField (bytes, 24, maxManifestBytes + 1);
    zip = open (bytes);
    check (readManifest (*zip, json).failed(), "oversized declared manifest");

    for (const auto declared : { 1u, 3u })
    {
        bytes = archive ({ "manifest.json" });
        changeCentralField (bytes, 24, declared);
        zip = open (bytes);
        check (readManifest (*zip, json).failed(), "manifest actual size must match declaration");
    }
    bytes = archive ({ "manifest.json" }, std::string (maxManifestBytes + 1, 'x'));
    changeCentralField (bytes, 24, 1);
    zip = open (bytes);
    check (readManifest (*zip, json).failed(), "actual manifest expansion is bounded even with forged size");

    bytes = archive ({ "a", "b", "c", "d", "e" });
    changeCentralField (bytes, 24, 0xffffffffu);
    zip = open (bytes);
    check (validate (*zip).failed(), "aggregate expanded size");

    std::vector<juce::String> names;
    for (int i = 0; i < maxEntries; ++i)
        names.push_back (juce::String (i));
    zip = open (archive (names));
    check (validate (*zip).wasOk(), "entry count at limit");
    names.push_back ("extra");
    zip = open (archive (names));
    check (validate (*zip).failed(), "entry count above limit");

    bytes = archive ({ "project.lynx" });
    changeCentralField (bytes, 38, 0120000u << 16);
    zip = open (bytes);
    check (validate (*zip).failed(), "symbolic link rejected before extraction");

    zip = open (archive ({ "../escaped.lynx" }));
    const auto badFolder = root.getChildFile ("bad");
    badFolder.createDirectory();
    badFolder.getChildFile ("partial").replaceWithText ("partial");
    check (extract (*zip, badFolder).failed() && ! badFolder.exists()
               && ! root.getChildFile ("escaped.lynx").exists(), "traversal rejected and partial output removed");

    bytes = archive ({ "project.lynx" });
    changeCentralField (bytes, 24, 1);
    zip = open (bytes);
    check (extract (*zip, badFolder).failed() && ! badFolder.exists(), "forged size cannot write excess output");
    check (root.getChildFile ("valid/project.lynx").existsAsFile(), "failed extraction preserves other projects");
    return failures == 0 ? 0 : 1;
}
