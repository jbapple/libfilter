import java.util.concurrent.ThreadLocalRandom;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.assertFalse;
import org.junit.Test;
import com.github.jbapple.libfilter.BlockFilter;
import java.util.ArrayList;

public class BlockFilterTest {
  public BlockFilterTest() {}

  @Test
  public void InsertPersists() {
    int ndv = 16000;
    BlockFilter x = BlockFilter.CreateWithBytes(ndv);
    ArrayList<Long> hashes = new ArrayList<Long>(ndv);
    ThreadLocalRandom r = ThreadLocalRandom.current();
    for (int i = 0; i < ndv; ++i) {
      hashes.add(r.nextLong());
    }
    for (int i = 0; i < ndv; ++i) {
      x.AddHash64(hashes.get(i));
      for (int j = 0; j <= i; ++j) {
        assertTrue(x.FindHash64(hashes.get(j)));
      }
    }
  }

  @Test
  public void StartEmpty() {
    int ndv = 16000000;
    BlockFilter x = BlockFilter.CreateWithBytes(ndv);
    ThreadLocalRandom r = ThreadLocalRandom.current();
    for (int j = 0; j < ndv; ++j) {
      long v = r.nextLong();
      assertFalse(x.FindHash64(v));
    }
  }
}
