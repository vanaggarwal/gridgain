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

package org.gridgain.grid.spi.indexing.h2;

import org.gridgain.grid.*;
import org.gridgain.grid.spi.*;
import org.gridgain.grid.util.typedef.internal.*;


/**
 * Iterator over result set.
 */
abstract class GridH2ResultSetIterator<T> implements GridSpiCloseableIterator<T> {
    /** */
    private static final long serialVersionUID = 0L;

    /** */
    private Object[][] data;

    /** */
    private int idx;

    /**
     * @param data Data array.
     */
    protected GridH2ResultSetIterator(Object[][] data) {
        assert data != null;

        this.data = data;
    }

    /** {@inheritDoc} */
    @Override public boolean hasNext() {
        return idx < data.length && data[idx] != null;
    }

    /** {@inheritDoc} */
    @SuppressWarnings("IteratorNextCanNotThrowNoSuchElementException")
    @Override public T next() {
        return createRow(data[idx++]);
    }

    /**
     * @param row Row source.
     * @return Row.
     */
    protected abstract T createRow(Object[] row);

    /** {@inheritDoc} */
    @Override public void remove() {
        throw new UnsupportedOperationException();
    }

    /** {@inheritDoc} */
    @Override public void close() throws GridException {
        data = null;
    }

    /** {@inheritDoc} */
    @Override public String toString() {
        return S.toString((Class<GridH2ResultSetIterator>)getClass(), this);
    }
}
