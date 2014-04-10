/* @java.file.header */

/*  _________        _____ __________________        _____
 *  __  ____/___________(_)______  /__  ____/______ ____(_)_______
 *  _  / __  __  ___/__  / _  __  / _  / __  _  __ `/__  / __  __ \
 *  / /_/ /  _  /    _  /  / /_/ /  / /_/ /  / /_/ / _  /  _  / / /
 *  \____/   /_/     /_/   \_,__/   \____/   \__,_/  /_/   /_/ /_/
 */

package org.gridgain.grid.kernal.processors.cache.distributed.replicated;

import org.gridgain.grid.cache.*;
import org.gridgain.grid.kernal.processors.cache.*;

import static org.gridgain.grid.cache.GridCacheMode.*;

/**
 * Multi-node test for group locking in replicated cache.
 */
public class GridCacheGroupLockMultiNodeReplicatedSelfTest extends
    GridCacheGroupLockMultiNodeAbstractSelfTest {
    /** {@inheritDoc} */
    @Override protected boolean nearEnabled() {
        // Near is not defined for replicated cache.
        return false;
    }

    /** {@inheritDoc} */
    @Override protected GridCacheMode cacheMode() {
        return REPLICATED;
    }

    /** {@inheritDoc} */
    @Override public void testGroupLockWithExternalLockOptimistic() {
        // TODO: GG-6333
    }
}