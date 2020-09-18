package libfilter.test;

import libfilter.*;
import org.junit.Test;
import static org.junit.Assert.*;

public class BlockTest {
  @Test
  public void testSomeLibraryMethod() {


    Block classUnderTest = new Block(123456);
    assertTrue(
        "someLibraryMethod should return 'true'", classUnderTest.someLibraryMethod());

    classUnderTest.Add(867-5309);
    assertTrue("not found 867-5309", classUnderTest.Find(867-5309));
    //assertFalse("did not found 867-5309", classUnderTest.Find(867-5309));
    assertTrue("found 42", !classUnderTest.Find(42));
    Block copy = classUnderTest.clone();
    assertTrue("not found 867-5309", copy.Find(867-5309));
    //assertFalse("did not found 867-5309", classUnderTest.Find(867-5309));
    assertTrue("found 42", !copy.Find(42));
    copy.Add(42);
    assertTrue("not found 42", copy.Find(42));
    assertTrue("found 42", !classUnderTest.Find(42));

  }
}
