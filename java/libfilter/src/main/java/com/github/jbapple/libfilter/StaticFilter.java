package com.github.jbapple.libfilter;

/**
 * Filter represents approximate membership query objects.
 */
public interface StaticFilter {
  /**
   * Find a hash value in the filter.
   * <p>
   * Do not mix with <code>AddHash32</code> - a hash value that is added with
   * <code>AddHash32</code> will not be present when calling <code>FindHash64</code>.
   * <p>
   * Do not pass values to this function, only their hashes.
   * <p>
   * <em>Hashes must be 64-bits</em>. 32-bit hashes will return incorrect results.
   *
   * @param hash the 64-bit hash value of the element you are checking the presence of
   */
  boolean FindHash64(long hash);

  /**
   * Find a hash value in the filter.
   * <p>
   * Do not mix with <code>AddHash64</code> - a hash value that is added with
   * <code>AddHash64</code> will not be present when calling <code>FindHash32</code>.
   *
   * @param hash the 32-bit hash value of the element you are checking the presence of
   */
  boolean FindHash32(int hash);
}
