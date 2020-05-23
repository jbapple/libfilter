* Function multi-versioning to work with or without SIMD with decision at run-time
* Compile-time decision on whether to use various SIMD ISA's, like ARM or SSE (not just AVX2 or nothing)
* Windows and BSD compatibility
* random words within a page that is the size of a cache line
* gcc's vector types, and let the compiler figure it out. ABI change? Use _Generic.
* write straigh-forward code (except for the check at the end of lookup) and let the compiler autovectorize
* segmented memory - one large page and then some regular sized pages
