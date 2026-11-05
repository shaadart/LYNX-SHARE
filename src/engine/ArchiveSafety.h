#pragma once

#include <juce_core/juce_core.h>

namespace lynx::archivesafety
{
constexpr int maxEntries = 10000;
constexpr juce::int64 maxExpandedBytes = 16LL * 1024 * 1024 * 1024;
constexpr int maxManifestBytes = 4 * 1024 * 1024;

inline juce::Result validate (const juce::ZipFile& zip)
{
    if (zip.getNumEntries() == 0 || zip.getNumEntries() > maxEntries)
        return juce::Result::fail ("the package must contain 1 to 10000 entries");
    juce::int64 total = 0;
    for (int i = 0; i < zip.getNumEntries(); ++i)
    {
        const auto& entry = *zip.getEntry (i);
        if (entry.isSymbolicLink) // JUCE would create it; copying media through it could read files outside the package
            return juce::Result::fail ("the package contains a link");
        if (entry.uncompressedSize < 0 || entry.uncompressedSize > maxExpandedBytes - total)
            return juce::Result::fail ("the package expands beyond 16 GiB");
        total += entry.uncompressedSize;
    }
    return juce::Result::ok();
}

// The caller owns this new folder; failed extraction must not leave partial files behind.
inline juce::Result extract (juce::ZipFile& zip, const juce::File& folder)
{
    auto result = validate (zip);
    if (result.wasOk())
        result = zip.uncompressTo (folder, false); // JUCE 8.0.17 also bounds actual output to each declared size
    if (result.failed())
        folder.deleteRecursively();
    return result;
}

inline juce::Result readManifest (juce::ZipFile& zip, juce::String& json)
{
    if (const auto result = validate (zip); result.failed())
        return result;
    const auto index = zip.getIndexOfFileName ("manifest.json");
    if (index < 0)
        return juce::Result::fail ("not a LYNX SHARE package (no manifest)");
    const auto size = zip.getEntry (index)->uncompressedSize;
    if (size > maxManifestBytes)
        return juce::Result::fail ("the package manifest exceeds 4 MiB");
    std::unique_ptr<juce::InputStream> in (zip.createStreamForEntry (index));
    if (in == nullptr)
        return juce::Result::fail ("could not read the package");
    juce::MemoryBlock bytes;
    in->readIntoMemoryBlock (bytes, maxManifestBytes + 1);
    char extra = 0; // readIntoMemoryBlock may stop at the untrusted declared length
    if (bytes.getSize() > maxManifestBytes || (juce::int64) bytes.getSize() != size || in->read (&extra, 1) != 0)
        return juce::Result::fail ("the package manifest size is invalid");
    json = juce::String::fromUTF8 ((const char*) bytes.getData(), (int) bytes.getSize());
    return juce::Result::ok();
}
} // namespace lynx::archivesafety
