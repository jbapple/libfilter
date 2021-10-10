package com.github.jbapple.libfilter;

/**
 * Filter represents approximate membership query objects.
 */
public interface Filter {
  /**
   * Add a hash value to the filter.
   * <p>
   * Do not mix with <code>FindHash32</code> - a hash value that is present with
   * <code>FindHash64(x)</code> will not be present when calling
   * <code>FindHash32((int)x)</code>.
   * <p>
   * Do not pass values to this function, only their hashes.
   * <p>
   * <em>Hashes must be 64-bits</em>. 32-bit hashes will result in a useless filter where
   * <code>FindHash64</code> always returns <code>true</code> for any input.
   *
   * @param hash the 64-bit hash value of the element you wish to insert
   */
  public boolean AddHash64(long hash);

  /**
   * Find a hash value in the filter.
   * <p>
   * Do not mix with <code>AddHash32</code> - a hash value that is added with
   * <code>AddHash32</code> will not be present when calling <code>FindHash64</code>.
   * <p>
   * Do not pass values to this funciton, only their hashes.
   * <p>
   * <em>Hashes must be 64-bits</em>. 32-bit hashes will return incorrect results.
   *
   * @param hash the 64-bit hash value of the element you are checking the presence of
   */
  public boolean FindHash64(long hash);

  /**
   * Add a hash value to the filter.
   * <p>
   * Do not mix with <code>FindHash64</code> - a hash value that is present with
   * <code>FindHash64(x)</code> will not be present when calling
   * <code>FindHash32((int)x)</code>.
   *
   * @param hash the 32-bit hash value of the element you wish to insert
   */
  public boolean AddHash32(int hash);

  /**
   * Find a hash value in the filter.
   * <p>
   * Do not mix with <code>AddHash64</code> - a hash value that is added with
   * <code>AddHash64</code> will not be present when calling <code>FindHash32</code>.
   *
   * @param hash the 32-bit hash value of the element you are checking the presence of
   */
  public boolean FindHash32(int hash);
}
