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

package org.gridgain.grid.kernal.processors.hadoop.taskexecutor.external.child;

import org.gridgain.grid.kernal.processors.hadoop.taskexecutor.external.*;
import org.gridgain.grid.kernal.processors.hadoop.taskexecutor.external.communication.*;
import org.gridgain.grid.logger.*;
import org.gridgain.grid.logger.java.*;
import org.gridgain.grid.marshaller.optimized.*;
import org.gridgain.grid.util.ipc.shmem.*;
import org.gridgain.grid.util.typedef.internal.*;
import org.gridgain.grid.util.worker.*;

import java.io.*;
import java.text.*;
import java.util.*;
import java.util.concurrent.*;
import java.util.logging.*;
import java.util.logging.Formatter;

/**
 * Hadoop external process base class.
 */
public class GridHadoopExternalProcessStarter {
    /** Arguments. */
    private Args args;

    /** System out. */
    private OutputStream out;

    /** System err. */
    private OutputStream err;

    /**
     * @param args Parsed arguments.
     */
    public GridHadoopExternalProcessStarter(Args args) {
        this.args = args;
    }

    /**
     * @param cmdArgs Process arguments.
     */
    public static void main(String[] cmdArgs) {
        try {
            Args args = arguments(cmdArgs);

            new GridHadoopExternalProcessStarter(args).run();
        }
        catch (Exception e) {
            System.err.println("Failed");

            System.err.println(e.getMessage());

            e.printStackTrace(System.err);
        }
    }

    /**
     *
     * @throws Exception
     */
    public void run() throws Exception {
        U.setWorkDirectory(args.workDir, U.getGridGainHome());

        initializeStreams();

        ExecutorService msgExecSvc = Executors.newFixedThreadPool(
            Integer.getInteger("MSG_THREAD_POOL_SIZE", Runtime.getRuntime().availableProcessors() * 2));

        GridLogger log = logger();

        GridHadoopExternalCommunication comm = new GridHadoopExternalCommunication(
            args.nodeId,
            args.childProcId,
            new GridOptimizedMarshaller(),
            log,
            msgExecSvc,
            "external"
        );

        comm.start();

        GridHadoopProcessDescriptor nodeDesc = new GridHadoopProcessDescriptor(args.nodeId, args.parentProcId);
        nodeDesc.address(args.addr);
        nodeDesc.tcpPort(args.tcpPort);
        nodeDesc.sharedMemoryPort(args.shmemPort);

        GridHadoopChildProcessRunner runner = new GridHadoopChildProcessRunner();

        runner.start(comm, nodeDesc, msgExecSvc, log);

        System.err.println("Started");
        System.err.flush();

        System.setOut(new PrintStream(out));
        System.setErr(new PrintStream(err));
    }

    /**
     * @throws Exception
     */
    private void initializeStreams() throws Exception {
        File f = new File(args.out);

        if (!f.exists()) {
            if (!f.mkdirs())
                throw new IOException("Failed to create output directory: " + args.out);
        }
        else {
            if (f.isFile())
                throw new IOException("Output directory is a file: " + args.out);
        }

        out = new FileOutputStream(new File(f, args.childProcId + ".out"));
        err = new FileOutputStream(new File(f, args.childProcId + ".err"));
    }

    /**
     * TODO configure logger.
     *
     * @return Logger.
     * @throws IOException If failed.
     */
    private GridLogger logger() throws IOException {
        Logger log = Logger.getLogger("");

        log.setLevel(Level.FINE);

        for (Handler h : log.getHandlers())
            log.removeHandler(h);

        FileHandler h = new FileHandler(args.out + File.separator + args.childProcId + ".log", true);

        h.setFormatter(new Formatter() {
            @Override public String format(LogRecord record) {
                StringBuffer sb = new StringBuffer();

                DateFormat df = new SimpleDateFormat("yy-MM-dd HH:mm:ss.SSS");

                sb.append("[");

                df.format(new Date(record.getMillis()), sb, new FieldPosition(0));

                sb.append("][").append(record.getLevel()).append("][")
                    .append(record.getLoggerName()).append("] ").append(record.getMessage()).append("\n");

                if (record.getThrown() != null) {
                    StringWriter sw = new StringWriter();

                    PrintWriter pw = new PrintWriter(sw);

                    record.getThrown().printStackTrace(pw);

                    pw.flush();

                    sb.append(sw.toString());
                }

                return sb.toString();
            }
        });

        log.addHandler(h);

        Logger.getLogger(GridIpcSharedMemorySpace.class.toString()).setLevel(Level.WARNING);
        Logger.getLogger(GridIpcSharedMemorySpace.class.getName()).setLevel(Level.WARNING);
        Logger.getLogger(GridWorker.class.toString()).setLevel(Level.WARNING);
        Logger.getLogger(GridWorker.class.getName()).setLevel(Level.WARNING);

        return new GridJavaLogger(log);
    }

    /**
     * @param processArgs Process arguments.
     * @return Child process instance.
     */
    private static Args arguments(String[] processArgs) throws Exception {
        Args args = new Args();

        for (int i = 0; i < processArgs.length; i++) {
            String arg = processArgs[i];

            switch (arg) {
                case "-cpid": {
                    if (i == processArgs.length - 1)
                        throw new Exception("Missing process ID for '-cpid' parameter");

                    String procIdStr = processArgs[++i];

                    args.childProcId = UUID.fromString(procIdStr);

                    break;
                }

                case "-ppid": {
                    if (i == processArgs.length - 1)
                        throw new Exception("Missing process ID for '-ppid' parameter");

                    String procIdStr = processArgs[++i];

                    args.parentProcId = UUID.fromString(procIdStr);

                    break;
                }

                case "-nid": {
                    if (i == processArgs.length - 1)
                        throw new Exception("Missing node ID for '-nid' parameter");

                    String nodeIdStr = processArgs[++i];

                    args.nodeId = UUID.fromString(nodeIdStr);

                    break;
                }

                case "-addr": {
                    if (i == processArgs.length - 1)
                        throw new Exception("Missing node address for '-addr' parameter");

                    args.addr = processArgs[++i];

                    break;
                }

                case "-tport": {
                    if (i == processArgs.length - 1)
                        throw new Exception("Missing tcp port for '-tport' parameter");

                    args.tcpPort = Integer.parseInt(processArgs[++i]);

                    break;
                }

                case "-sport": {
                    if (i == processArgs.length - 1)
                        throw new Exception("Missing shared memory port for '-sport' parameter");

                    args.shmemPort = Integer.parseInt(processArgs[++i]);

                    break;
                }

                case "-out": {
                    if (i == processArgs.length - 1)
                        throw new Exception("Missing output folder name for '-out' parameter");

                    args.out = processArgs[++i];

                    break;
                }

                case "-wd": {
                    if (i == processArgs.length - 1)
                        throw new Exception("Missing work folder name for '-wd' parameter");

                    args.workDir = processArgs[++i];

                    break;
                }
            }
        }

        return args;
    }

    /**
     * Execution arguments.
     */
    private static class Args {
        /** Process ID. */
        private UUID childProcId;

        /** Process ID. */
        private UUID parentProcId;

        /** Process ID. */
        private UUID nodeId;

        /** Node address. */
        private String addr;

        /** TCP port */
        private int tcpPort;

        /** Shmem port. */
        private int shmemPort = -1;

        /** Output folder. */
        private String out;

        /** Work directory. */
        private String workDir;
    }
}
