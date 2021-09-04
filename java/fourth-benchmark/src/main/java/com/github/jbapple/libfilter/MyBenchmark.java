/*
 * Copyright (c) 2014, Oracle America, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 *  * Neither the name of Oracle nor the names of its contributors may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

package com.github.jbapple.libfilter;

import org.openjdk.jmh.annotations.*;
import org.openjdk.jmh.runner.*;
import org.openjdk.jmh.runner.options.*;

import java.util.concurrent.TimeUnit;

@BenchmarkMode(Mode.SampleTime)
@Warmup(iterations = 0, time = 1, timeUnit = TimeUnit.SECONDS)
@Fork(1)
public class MyBenchmark {
  @State(Scope.Thread)
  public static class MyState {
    @Param({"0", "1", "2", "3", "4", "5", "6", "7", "8"})
    int exponent;
    BlockFilter b;
    TaffyBlockFilter t;
    public MyState() {
      b = BlockFilter.CreateWithNdvFpp(Math.pow(10, exponent), 0.001);
      t = TaffyBlockFilter.CreateWithNdvFpp(1, 0.001);
    }
  }

  @Benchmark
  @OutputTimeUnit(TimeUnit.MICROSECONDS)
  @Measurement(iterations = 1, time = 10, timeUnit = TimeUnit.SECONDS)
  public TaffyBlockFilter BenchTaffyBlock(MyState s) {
    TaffyBlockFilter t = s.t.clone();
    for (long i = 0; i <  Math.pow(10, s.exponent); ++i) {
       t.AddHash64(i);
    }
    return t;
  }

  @Benchmark
  @OutputTimeUnit(TimeUnit.MICROSECONDS)
  @Measurement(iterations = 1, time = 10, timeUnit = TimeUnit.SECONDS)
  public BlockFilter BenchBlock(MyState s) {
    BlockFilter b = s.b.clone();
    for (long i = 0; i <  Math.pow(10, s.exponent); ++i) {
       b.AddHash64(i);
    }
    return b;
  }

  @Benchmark
  @OutputTimeUnit(TimeUnit.MICROSECONDS)
  @Measurement(iterations = 1, time = 10, timeUnit = TimeUnit.SECONDS)
  public BlockFilter BenchBlockClone(MyState s) {
    return s.b.clone();
  }

  // @Benchmark
  // @OutputTimeUnit(TimeUnit.MICROSECONDS)
  // @Measurement(iterations = 30, time = 1, timeUnit = TimeUnit.SECONDS)
  // public BlockFilter BenchBlockNdvFpp(MyState s) {
  //   BlockFilter b = BlockFilter.CreateWithNdvFpp(1000 * 1000, 0.001);
  //   for (long i = 0; i <  1000 * 1000; ++i) {
  //      b.AddHash64(i);
  //   }
  //   return b;
  // }
}
