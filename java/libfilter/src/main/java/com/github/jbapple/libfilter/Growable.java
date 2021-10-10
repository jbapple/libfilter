package com.github.jbapple.libfilter;

public interface Growable {
  /**
   * Increase the size of the filter.
   * <p>
   * This also happens automatically when an <code>Insert</code> operation is attempted on
   * a nearly-full filter. Calling <code>Upsize</code> before then can prep the filter to
   * have capacity enough to perform more <code>Insert</code> operations without
   * triggering an <code>Upsize</code> call.
   */
  public void Upsize();
}
