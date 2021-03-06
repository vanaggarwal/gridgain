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

package org.gridgain.client.hadoop;

import org.apache.hadoop.conf.*;
import org.apache.hadoop.fs.*;
import org.apache.hadoop.io.*;
import org.apache.hadoop.mapreduce.*;
import org.apache.hadoop.mapreduce.lib.input.*;
import org.apache.hadoop.mapreduce.lib.output.*;
import org.apache.hadoop.mapreduce.protocol.*;
import org.gridgain.grid.ggfs.*;
import org.gridgain.grid.kernal.processors.hadoop.*;
import org.gridgain.grid.util.lang.*;
import org.gridgain.grid.util.typedef.*;
import org.gridgain.grid.util.typedef.internal.*;
import org.gridgain.testframework.*;

import java.io.*;
import java.util.*;

/**
 * Hadoop client protocol tests in external process mode.
 */
@SuppressWarnings("ResultOfMethodCallIgnored")
public class GridHadoopClientProtocolSelfTest extends GridHadoopAbstractSelfTest {
    /** Input path. */
    private static final String PATH_INPUT = "/input";

    /** Output path. */
    private static final String PATH_OUTPUT = "/output";

    /** Job name. */
    private static final String JOB_NAME = "myJob";

    /** Setup lock file. */
    private static File setupLockFile = new File(U.isWindows() ? System.getProperty("java.io.tmpdir") : "/tmp",
        "gg-lock-setup.file");

    /** Map lock file. */
    private static File mapLockFile = new File(U.isWindows() ? System.getProperty("java.io.tmpdir") : "/tmp",
        "gg-lock-map.file");

    /** Reduce lock file. */
    private static File reduceLockFile = new File(U.isWindows() ? System.getProperty("java.io.tmpdir") : "/tmp",
        "gg-lock-reduce.file");

    /** {@inheritDoc} */
    @Override protected int gridCount() {
        return 2;
    }

    /** {@inheritDoc} */
    @Override protected boolean ggfsEnabled() {
        return true;
    }

    /** {@inheritDoc} */
    @Override protected boolean restEnabled() {
        return true;
    }

    /** {@inheritDoc} */
    @Override protected void beforeTestsStarted() throws Exception {
        super.beforeTestsStarted();

        startGrids(gridCount());

        setupLockFile.delete();
        mapLockFile.delete();
        reduceLockFile.delete();
    }

    /** {@inheritDoc} */
    @Override protected void afterTestsStopped() throws Exception {
        stopAllGrids();

        super.afterTestsStopped();

//        GridHadoopClientProtocolProvider.cliMap.clear();
    }

    /** {@inheritDoc} */
    @Override protected void beforeTest() throws Exception {
        setupLockFile.createNewFile();
        mapLockFile.createNewFile();
        reduceLockFile.createNewFile();

        setupLockFile.deleteOnExit();
        mapLockFile.deleteOnExit();
        reduceLockFile.deleteOnExit();

        super.beforeTest();
    }

    /** {@inheritDoc} */
    @Override protected void afterTest() throws Exception {
        grid(0).ggfs(GridHadoopAbstractSelfTest.ggfsName).format().get();

        setupLockFile.delete();
        mapLockFile.delete();
        reduceLockFile.delete();

        super.afterTest();
    }

    /**
     * Test next job ID generation.
     *
     * @throws Exception If failed.
     */
    @SuppressWarnings("ConstantConditions")
    public void testNextJobId() throws Exception {
        GridHadoopClientProtocolProvider provider = provider();

        ClientProtocol proto = provider.create(config(GridHadoopAbstractSelfTest.REST_PORT));

        JobID jobId = proto.getNewJobID();

        assert jobId != null;
        assert jobId.getJtIdentifier() != null;

        JobID nextJobId = proto.getNewJobID();

        assert nextJobId != null;
        assert nextJobId.getJtIdentifier() != null;

        assert !F.eq(jobId, nextJobId);
    }

    /**
     * Tests job counters retrieval.
     *
     * @throws Exception If failed.
     */
    public void testJobCounters() throws Exception {
        GridGgfs ggfs = grid(0).ggfs(GridHadoopAbstractSelfTest.ggfsName);

        ggfs.mkdirs(new GridGgfsPath(PATH_INPUT));

        try (BufferedWriter bw = new BufferedWriter(new OutputStreamWriter(ggfs.create(
            new GridGgfsPath(PATH_INPUT + "/test.file"), true)))) {

            bw.write(
                "alpha\n" +
                "beta\n" +
                "gamma\n" +
                "alpha\n" +
                "beta\n" +
                "gamma\n" +
                "alpha\n" +
                "beta\n" +
                "gamma\n"
            );
        }

        Configuration conf = config(GridHadoopAbstractSelfTest.REST_PORT);

        final Job job = Job.getInstance(conf);

        job.setOutputKeyClass(Text.class);
        job.setOutputValueClass(IntWritable.class);

        job.setMapperClass(TestCountingMapper.class);
        job.setReducerClass(TestCountingReducer.class);
        job.setCombinerClass(TestCountingCombiner.class);

        FileInputFormat.setInputPaths(job, new Path(PATH_INPUT));
        FileOutputFormat.setOutputPath(job, new Path(PATH_OUTPUT));

        job.submit();

        final Counter counter = job.getCounters().findCounter(TestCounter.COUNTER1);

        assertEquals(0, counter.getValue());

        counter.increment(10);

        assertEquals(10, counter.getValue());

        // Transferring to map phase.
        setupLockFile.delete();

        // Transferring to reduce phase.
        mapLockFile.delete();

        job.waitForCompletion(false);

        assertEquals("job must end successfully", JobStatus.State.SUCCEEDED, job.getStatus().getState());

        final Counters counters = job.getCounters();

        assertNotNull("counters cannot be null", counters);
        assertEquals("wrong counters count", 3, counters.countCounters());
        assertEquals("wrong counter value", 15, counters.findCounter(TestCounter.COUNTER1).getValue());
        assertEquals("wrong counter value", 3, counters.findCounter(TestCounter.COUNTER2).getValue());
        assertEquals("wrong counter value", 3, counters.findCounter(TestCounter.COUNTER3).getValue());
    }

    /**
     * Tests job counters retrieval for unknown job id.
     *
     * @throws Exception If failed.
     */
    public void testUnknownJobCounters() throws Exception {
        GridHadoopClientProtocolProvider provider = provider();

        ClientProtocol proto = provider.create(config(GridHadoopAbstractSelfTest.REST_PORT));

        try {
            proto.getJobCounters(new JobID(UUID.randomUUID().toString(), -1));
            fail("exception must be thrown");
        }
        catch (Exception e) {
            assert e instanceof IOException : "wrong error has been thrown";
        }
    }

    /**
     * @throws Exception If failed.
     */
    public void testJobSubmitMap() throws Exception {
        checkJobSubmit(true, true);
    }

    /**
     * @throws Exception If failed.
     */
    public void testJobSubmitMapCombine() throws Exception {
        checkJobSubmit(false, true);
    }

    /**
     * @throws Exception If failed.
     */
    public void testJobSubmitMapReduce() throws Exception {
        checkJobSubmit(true, false);
    }

    /**
     * @throws Exception If failed.
     */
    public void testJobSubmitMapCombineReduce() throws Exception {
        checkJobSubmit(false, false);
    }

    /**
     * Test job submission.
     *
     * @param noCombiners Whether there are no combiners.
     * @param noReducers Whether there are no reducers.
     * @throws Exception If failed.
     */
    public void checkJobSubmit(boolean noCombiners, boolean noReducers) throws Exception {
        GridGgfs ggfs = grid(0).ggfs(GridHadoopAbstractSelfTest.ggfsName);

        ggfs.mkdirs(new GridGgfsPath(PATH_INPUT));

        try (BufferedWriter bw = new BufferedWriter(new OutputStreamWriter(ggfs.create(
            new GridGgfsPath(PATH_INPUT + "/test.file"), true)))) {

            bw.write("word");
        }

        Configuration conf = config(GridHadoopAbstractSelfTest.REST_PORT);

        final Job job = Job.getInstance(conf);

        job.setJobName(JOB_NAME);

        job.setOutputKeyClass(Text.class);
        job.setOutputValueClass(IntWritable.class);

        job.setMapperClass(TestMapper.class);
        job.setReducerClass(TestReducer.class);

        if (!noCombiners)
            job.setCombinerClass(TestCombiner.class);

        if (noReducers)
            job.setNumReduceTasks(0);

        job.setInputFormatClass(TextInputFormat.class);
        job.setOutputFormatClass(TestOutputFormat.class);

        FileInputFormat.setInputPaths(job, new Path(PATH_INPUT));
        FileOutputFormat.setOutputPath(job, new Path(PATH_OUTPUT));

        job.submit();

        JobID jobId = job.getJobID();

        // Setup phase.
        JobStatus jobStatus = job.getStatus();
        checkJobStatus(jobStatus, jobId, JOB_NAME, JobStatus.State.RUNNING, 0.0f);
        assert jobStatus.getSetupProgress() >= 0.0f && jobStatus.getSetupProgress() < 1.0f;
        assert jobStatus.getMapProgress() == 0.0f;
        assert jobStatus.getReduceProgress() == 0.0f;

        U.sleep(2100);

        JobStatus recentJobStatus = job.getStatus();

        assert recentJobStatus.getSetupProgress() > jobStatus.getSetupProgress() :
            "Old=" + jobStatus.getSetupProgress() + ", new=" + recentJobStatus.getSetupProgress();

        // Transferring to map phase.
        setupLockFile.delete();

        assert GridTestUtils.waitForCondition(new GridAbsPredicate() {
            @Override public boolean apply() {
                try {
                    return F.eq(1.0f, job.getStatus().getSetupProgress());
                }
                catch (Exception e) {
                    throw new RuntimeException("Unexpected exception.", e);
                }
            }
        }, 5000L);

        // Map phase.
        jobStatus = job.getStatus();
        checkJobStatus(jobStatus, jobId, JOB_NAME, JobStatus.State.RUNNING, 0.0f);
        assert jobStatus.getSetupProgress() == 1.0f;
        assert jobStatus.getMapProgress() >= 0.0f && jobStatus.getMapProgress() < 1.0f;
        assert jobStatus.getReduceProgress() == 0.0f;

        U.sleep(2100);

        recentJobStatus = job.getStatus();

        assert recentJobStatus.getMapProgress() > jobStatus.getMapProgress() :
            "Old=" + jobStatus.getMapProgress() + ", new=" + recentJobStatus.getMapProgress();

        // Transferring to reduce phase.
        mapLockFile.delete();

        assert GridTestUtils.waitForCondition(new GridAbsPredicate() {
            @Override public boolean apply() {
                try {
                    return F.eq(1.0f, job.getStatus().getMapProgress());
                }
                catch (Exception e) {
                    throw new RuntimeException("Unexpected exception.", e);
                }
            }
        }, 5000L);

        if (!noReducers) {
            // Reduce phase.
            jobStatus = job.getStatus();
            checkJobStatus(jobStatus, jobId, JOB_NAME, JobStatus.State.RUNNING, 0.0f);
            assert jobStatus.getSetupProgress() == 1.0f;
            assert jobStatus.getMapProgress() == 1.0f;
            assert jobStatus.getReduceProgress() >= 0.0f && jobStatus.getReduceProgress() < 1.0f;

            // Ensure that reduces progress increases.
            U.sleep(2100);

            recentJobStatus = job.getStatus();

            assert recentJobStatus.getReduceProgress() > jobStatus.getReduceProgress() :
                "Old=" + jobStatus.getReduceProgress() + ", new=" + recentJobStatus.getReduceProgress();

            reduceLockFile.delete();
        }

        job.waitForCompletion(false);

        jobStatus = job.getStatus();
        checkJobStatus(job.getStatus(), jobId, JOB_NAME, JobStatus.State.SUCCEEDED, 1.0f);
        assert jobStatus.getSetupProgress() == 1.0f;
        assert jobStatus.getMapProgress() == 1.0f;
        assert jobStatus.getReduceProgress() == 1.0f;

        dumpGgfs(ggfs, new GridGgfsPath(PATH_OUTPUT));
    }

    /**
     * Dump GGFS content.
     *
     * @param ggfs GGFS.
     * @param path Path.
     * @throws Exception If failed.
     */
    @SuppressWarnings("ConstantConditions")
    private static void dumpGgfs(GridGgfs ggfs, GridGgfsPath path) throws Exception {
        GridGgfsFile file = ggfs.info(path);

        assert file != null;

        System.out.println(file.path());

        if (file.isDirectory()) {
            for (GridGgfsPath child : ggfs.listPaths(path))
                dumpGgfs(ggfs, child);
        }
        else {
            try (BufferedReader br = new BufferedReader(new InputStreamReader(ggfs.open(path)))) {
                String line = br.readLine();

                while (line != null) {
                    System.out.println(line);

                    line = br.readLine();
                }
            }
        }
    }

    /**
     * Check job status.
     *
     * @param status Job status.
     * @param expJobId Expected job ID.
     * @param expJobName Expected job name.
     * @param expState Expected state.
     * @param expCleanupProgress Expected cleanup progress.
     * @throws Exception If failed.
     */
    private static void checkJobStatus(JobStatus status, JobID expJobId, String expJobName,
        JobStatus.State expState, float expCleanupProgress) throws Exception {
        assert F.eq(status.getJobID(), expJobId) : "Expected=" + expJobId + ", actual=" + status.getJobID();
        assert F.eq(status.getJobName(), expJobName) : "Expected=" + expJobName + ", actual=" + status.getJobName();
        assert F.eq(status.getState(), expState) : "Expected=" + expState + ", actual=" + status.getState();
        assert F.eq(status.getCleanupProgress(), expCleanupProgress) :
            "Expected=" + expCleanupProgress + ", actual=" + status.getCleanupProgress();
    }

    /**
     * @return Configuration.
     */
    private Configuration config(int port) {
        Configuration conf = new Configuration();

        setupFileSystems(conf);

        conf.set(MRConfig.FRAMEWORK_NAME, GridHadoopClientProtocol.FRAMEWORK_NAME);
        conf.set(MRConfig.MASTER_ADDRESS, "127.0.0.1:" + port);

        conf.set("fs.default.name", "ggfs://:" + getTestGridName(0) + "@/");

        return conf;
    }

    /**
     * @return Protocol provider.
     */
    private GridHadoopClientProtocolProvider provider() {
        return new GridHadoopClientProtocolProvider();
    }

    /**
     * Test mapper.
     */
    public static class TestMapper extends Mapper<Object, Text, Text, IntWritable> {
        /** Writable container for writing word. */
        private Text word = new Text();

        /** Writable integer constant of '1' is writing as count of found words. */
        private static final IntWritable one = new IntWritable(1);

        /** {@inheritDoc} */
        @Override public void map(Object key, Text val, Context ctx) throws IOException, InterruptedException {
            while (mapLockFile.exists())
                Thread.sleep(50);

            StringTokenizer wordList = new StringTokenizer(val.toString());

            while (wordList.hasMoreTokens()) {
                word.set(wordList.nextToken());

                ctx.write(word, one);
            }
        }
    }

    /**
     * Test Hadoop counters.
     */
    public enum TestCounter {
        COUNTER1, COUNTER2, COUNTER3
    }

    /**
     * Test mapper that uses counters.
     */
    public static class TestCountingMapper extends TestMapper {
        /** {@inheritDoc} */
        @Override public void map(Object key, Text val, Context ctx) throws IOException, InterruptedException {
            super.map(key, val, ctx);
            ctx.getCounter(TestCounter.COUNTER1).increment(1);
        }
    }

    /**
     * Test combiner that counts invocations.
     */
    public static class TestCountingCombiner extends TestReducer {
        @Override public void reduce(Text key, Iterable<IntWritable> values,
            Context ctx) throws IOException, InterruptedException {
            ctx.getCounter(TestCounter.COUNTER1).increment(1);
            ctx.getCounter(TestCounter.COUNTER2).increment(1);

            int sum = 0;
            for (IntWritable value : values) {
                sum += value.get();
            }

            ctx.write(key, new IntWritable(sum));
        }
    }

    /**
     * Test reducer that counts invocations.
     */
    public static class TestCountingReducer extends TestReducer {
        @Override public void reduce(Text key, Iterable<IntWritable> values,
            Context ctx) throws IOException, InterruptedException {
            ctx.getCounter(TestCounter.COUNTER1).increment(1);
            ctx.getCounter(TestCounter.COUNTER3).increment(1);
        }
    }

    /**
     * Test combiner.
     */
    public static class TestCombiner extends Reducer<Text, IntWritable, Text, IntWritable> {
        // No-op.
    }

    public static class TestOutputFormat<K, V> extends TextOutputFormat<K, V> {
        /** {@inheritDoc} */
        @Override public synchronized OutputCommitter getOutputCommitter(TaskAttemptContext ctx)
            throws IOException {
            return new TestOutputCommitter(ctx, (FileOutputCommitter)super.getOutputCommitter(ctx));
        }
    }

    /**
     * Test output committer.
     */
    private static class TestOutputCommitter extends FileOutputCommitter {
        /** Delegate. */
        private final FileOutputCommitter delegate;

        /**
         * Constructor.
         *
         * @param ctx Task attempt context.
         * @param delegate Delegate.
         * @throws IOException If failed.
         */
        private TestOutputCommitter(TaskAttemptContext ctx, FileOutputCommitter delegate) throws IOException {
            super(FileOutputFormat.getOutputPath(ctx), ctx);

            this.delegate = delegate;
        }

        /** {@inheritDoc} */
        @Override public void setupJob(JobContext jobCtx) throws IOException {
            try {
                while (setupLockFile.exists())
                    Thread.sleep(50);
            }
            catch (InterruptedException ignored) {
                throw new IOException("Interrupted.");
            }

            delegate.setupJob(jobCtx);
        }

        /** {@inheritDoc} */
        @Override public void setupTask(TaskAttemptContext taskCtx) throws IOException {
            delegate.setupTask(taskCtx);
        }

        /** {@inheritDoc} */
        @Override public boolean needsTaskCommit(TaskAttemptContext taskCtx) throws IOException {
            return delegate.needsTaskCommit(taskCtx);
        }

        /** {@inheritDoc} */
        @Override public void commitTask(TaskAttemptContext taskCtx) throws IOException {
            delegate.commitTask(taskCtx);
        }

        /** {@inheritDoc} */
        @Override public void abortTask(TaskAttemptContext taskCtx) throws IOException {
            delegate.abortTask(taskCtx);
        }
    }

    /**
     * Test reducer.
     */
    public static class TestReducer extends Reducer<Text, IntWritable, Text, IntWritable> {
        /** Writable container for writing sum of word counts. */
        private IntWritable totalWordCnt = new IntWritable();

        /** {@inheritDoc} */
        @Override public void reduce(Text key, Iterable<IntWritable> values, Context ctx) throws IOException,
            InterruptedException {
            while (reduceLockFile.exists())
                Thread.sleep(50);

            int wordCnt = 0;

            for (IntWritable value : values)
                wordCnt += value.get();

            totalWordCnt.set(wordCnt);

            ctx.write(key, totalWordCnt);
        }
    }
}
