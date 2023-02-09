#pragma once

namespace Server
{

/**
 * Kind of caching to apply.
 *
 * This is an abstraction of specific cache expiry times that could (at least in principle) be configured.
 */
enum class CacheKind
{
    /**
     * Data that is not cached at all.
     *
     * This should not be used for publicly accessible resources, but could be used for private resources such as a
     * management API.
     */
    none,

    /**
     * Data that changes often.
     *
     * The cache time for this is useful for information such as information about the current segment. A sensible cache
     * expiry for this kind is 1 second. In any case, the expiry time should be (significantly) less than the segment
     * pre-availability time.
     */
    ephemeral,

    /**
     * Data that is not expected to change.
     *
     * This is useful for fixed data that shouldn't be too inconvenient to erase from the cache, and might have a cache
     * expiry of 10 minutes or longer. This is useful for, for example, for serving a built-in streaming client.
     */
    fixed,

    /**
     * Data that must not be downloaded more than once.
     *
     * This is intended for large files that aren't expected to change or ever be uploaded twice, like the segments.
     * They should be protected by a unique ID that changes at startup so that restarts don't yield stale data from the
     * CDN (or browser) cache.
     *
     * The cache expiry it set to be longer than these resources are retained for so they don't get uploaded twice.
     */
    indefinite
};

} // namespace Server
