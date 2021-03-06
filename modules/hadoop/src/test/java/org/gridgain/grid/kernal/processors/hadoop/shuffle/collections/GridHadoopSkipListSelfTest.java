/* 
 Copyright (C) GridGain Systems. All Rights Reserved.
 
 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0
 
 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 */

/*  _________        _____ __________________        _____
 *  __  ____/___________(_)______  /__  ____/______ ____(_)_______
 *  _  / __  __  ___/__  / _  __  / _  / __  _  __ `/__  / __  __ \
 *  / /_/ /  _  /    _  /  / /_/ /  / /_/ /  / /_/ / _  /  _  / / /
 *  \____/   /_/     /_/   \_,__/   \____/   \__,_/  /_/   /_/ /_/
 */

package org.gridgain.grid.kernal.processors.hadoop.shuffle.collections;

import com.google.common.collect.*;
import org.apache.hadoop.io.*;
import org.gridgain.grid.hadoop.*;
import org.gridgain.grid.util.*;
import org.gridgain.grid.util.io.*;
import org.gridgain.grid.util.offheap.unsafe.*;
import org.gridgain.grid.util.typedef.*;

import java.io.*;
import java.util.*;
import java.util.concurrent.*;

import static java.lang.Math.*;
import static org.gridgain.grid.util.offheap.unsafe.GridUnsafeMemory.*;

/**
 * Skip list tests.
 */
public class GridHadoopSkipListSelfTest extends GridHadoopAbstractMapTest {
    /**
     *
     */
    public void testLevel() {
        Random rnd = new GridRandom();

        int[] levelsCnts = new int[32];

        int all = 10000;

        for (int i = 0; i < all; i++) {
            int level = GridHadoopSkipList.randomLevel(rnd);

            levelsCnts[level]++;
        }

        X.println("Distribution: " + Arrays.toString(levelsCnts));

        for (int level = 0; level < levelsCnts.length; level++) {
            int exp = (level + 1) == levelsCnts.length ? 0 : all >>> (level + 1);

            double precission = 0.72 / Math.max(32 >>> level, 1);

            int sigma = max((int)ceil(precission * exp), 5);

            X.println("Level: " + level + " exp: " + exp + " act: " + levelsCnts[level] + " precission: " + precission +
                " sigma: " + sigma);

            assertTrue(abs(exp - levelsCnts[level]) <= sigma);
        }
    }

    public void testMapSimple() throws Exception {
        GridUnsafeMemory mem = new GridUnsafeMemory(0);

//        mem.listen(new GridOffHeapEventListener() {
//            @Override public void onEvent(GridOffHeapEvent evt) {
//                if (evt == GridOffHeapEvent.ALLOCATE)
//                    U.dumpStack();
//            }
//        });

        Random rnd = new Random();

        int mapSize = 16 << rnd.nextInt(6);

        GridHadoopJob job = mockJob();

        GridHadoopTaskContext taskCtx = mockTaskContext(job);

        GridHadoopMultimap m = new GridHadoopSkipList(job, mem);

        GridHadoopConcurrentHashMultimap.Adder a = m.startAdding(taskCtx);

        Multimap<Integer, Integer> mm = ArrayListMultimap.create();
        Multimap<Integer, Integer> vis = ArrayListMultimap.create();

        for (int i = 0, vals = 4 * mapSize + rnd.nextInt(25); i < vals; i++) {
            int key = rnd.nextInt(mapSize);
            int val = rnd.nextInt();

            a.write(new IntWritable(key), new IntWritable(val));
            mm.put(key, val);

            X.println("k: " + key + " v: " + val);

            a.close();

            check(m, mm, vis, taskCtx);

            a = m.startAdding(taskCtx);
        }

//        a.add(new IntWritable(10), new IntWritable(2));
//        mm.put(10, 2);
//        check(m, mm);

        a.close();

        X.println("Alloc: " + mem.allocatedSize());

        m.close();

        assertEquals(0, mem.allocatedSize());
    }

    private void check(GridHadoopMultimap m, Multimap<Integer, Integer> mm, final Multimap<Integer, Integer> vis, GridHadoopTaskContext taskCtx)
        throws Exception {
        final GridHadoopTaskInput in = m.input(taskCtx);

        Map<Integer, Collection<Integer>> mmm = mm.asMap();

        int keys = 0;

        int prevKey = Integer.MIN_VALUE;

        while (in.next()) {
            keys++;

            IntWritable k = (IntWritable)in.key();

            assertNotNull(k);

            assertTrue(k.get() > prevKey);

            prevKey = k.get();

            Deque<Integer> vs = new LinkedList<>();

            Iterator<?> it = in.values();

            while (it.hasNext())
                vs.addFirst(((IntWritable) it.next()).get());

            Collection<Integer> exp = mmm.get(k.get());

            assertEquals(exp, vs);
        }

        assertEquals(mmm.size(), keys);

//!        assertEquals(m.keys(), keys);

        // Check visitor.

        final byte[] buf = new byte[4];

        final GridDataInput dataInput = new GridUnsafeDataInput();

        m.visit(false, new GridHadoopConcurrentHashMultimap.Visitor() {
            /** */
            IntWritable key = new IntWritable();

            /** */
            IntWritable val = new IntWritable();

            @Override public void onKey(long keyPtr, int keySize) {
                read(keyPtr, keySize, key);
            }

            @Override public void onValue(long valPtr, int valSize) {
                read(valPtr, valSize, val);

                vis.put(key.get(), val.get());
            }

            private void read(long ptr, int size, Writable w) {
                assert size == 4 : size;

                UNSAFE.copyMemory(null, ptr, buf, BYTE_ARR_OFF, size);

                dataInput.bytes(buf, size);

                try {
                    w.readFields(dataInput);
                }
                catch (IOException e) {
                    throw new RuntimeException(e);
                }
            }
        });

//        X.println("vis: " + vis);

        assertEquals(mm, vis);

        in.close();
    }

    /**
     * @throws Exception if failed.
     */
    public void testMultiThreaded() throws Exception {
        GridUnsafeMemory mem = new GridUnsafeMemory(0);

        X.println("___ Started");

        Random rnd = new GridRandom();

        for (int i = 0; i < 20; i++) {
            GridHadoopJob job = mockJob();

            final GridHadoopTaskContext taskCtx = mockTaskContext(job);

            final GridHadoopMultimap m = new GridHadoopSkipList(job, mem);

            final ConcurrentMap<Integer, Collection<Integer>> mm = new ConcurrentHashMap<>();

            X.println("___ MT");

            multithreaded(new Callable<Object>() {
                @Override public Object call() throws Exception {
                    X.println("___ TH in");

                    Random rnd = new GridRandom();

                    IntWritable key = new IntWritable();
                    IntWritable val = new IntWritable();

                    GridHadoopMultimap.Adder a = m.startAdding(taskCtx);

                    for (int i = 0; i < 50000; i++) {
                        int k = rnd.nextInt(32000);
                        int v = rnd.nextInt();

                        key.set(k);
                        val.set(v);

                        a.write(key, val);

                        Collection<Integer> list = mm.get(k);

                        if (list == null) {
                            list = new ConcurrentLinkedQueue<>();

                            Collection<Integer> old = mm.putIfAbsent(k, list);

                            if (old != null)
                                list = old;
                        }

                        list.add(v);
                    }

                    a.close();

                    X.println("___ TH out");

                    return null;
                }
            }, 3 + rnd.nextInt(27));

            GridHadoopTaskInput in = m.input(taskCtx);

            int prevKey = Integer.MIN_VALUE;

            while (in.next()) {
                IntWritable key = (IntWritable)in.key();

                assertTrue(key.get() > prevKey);

                prevKey = key.get();

                Iterator<?> valsIter = in.values();

                Collection<Integer> vals = mm.remove(key.get());

                assertNotNull(vals);

                while (valsIter.hasNext()) {
                    IntWritable val = (IntWritable) valsIter.next();

                    assertTrue(vals.remove(val.get()));
                }

                assertTrue(vals.isEmpty());
            }

            in.close();
            m.close();

            assertEquals(0, mem.allocatedSize());
        }
    }
}
