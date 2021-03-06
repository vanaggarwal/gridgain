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

package org.gridgain.grid.portables;

/**
 * Interface that allows to implement custom serialization/deserialization
 * logic for portable objects.
 */
public interface GridPortableMarshalAware {
    /**
     * Writes fields to provided writer.
     *
     * @param writer Portable object writer.
     * @throws GridPortableException In case of error.
     */
    public void writePortable(GridPortableWriter writer) throws GridPortableException;

    /**
     * Reads fields from provided reader.
     *
     * @param reader Portable object reader.
     * @throws GridPortableException In case of error.
     */
    public void readPortable(GridPortableReader reader) throws GridPortableException;
}
