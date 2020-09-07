//package libfilterTest;

import org.junit.Test;
import static org.junit.Assert.*;
//import com.jbapple.libfilter.Library;
//import libfilter.*;

public class LibraryTest {
  @Test
  public void testSomeLibraryMethod() {
    Library classUnderTest = new Library(123456);
    assertTrue(
        "someLibraryMethod should return 'true'", classUnderTest.someLibraryMethod());
    System.out.println("Before Add");
    classUnderTest.Add(867-5309);
    assertTrue("found 867-5309", classUnderTest.Find(867-5309));
    //assertFalse("did not found 867-5309", classUnderTest.Find(867-5309));
    //assertTrue("found 42", classUnderTest.Find(42));
  }
}
