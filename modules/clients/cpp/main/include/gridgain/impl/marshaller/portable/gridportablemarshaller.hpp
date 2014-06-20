/* @cpp.file.header */

/*  _________        _____ __________________        _____
 *  __  ____/___________(_)______  /__  ____/______ ____(_)_______
 *  _  / __  __  ___/__  / _  __  / _  / __  _  __ `/__  / __  __ \
 *  / /_/ /  _  /    _  /  / /_/ /  / /_/ /  / /_/ / _  /  _  / / /
 *  \____/   /_/     /_/   \_,__/   \____/   \__,_/  /_/   /_/ /_/
 */

#ifndef GRIDCLIENT_PORTABLE_MARSHALLER_HPP_INCLUDED
#define GRIDCLIENT_PORTABLE_MARSHALLER_HPP_INCLUDED

#include <vector>
#include <iterator>
#include <boost/algorithm/string.hpp>
#include <boost/detail/endian.hpp>

#include "gridgain/gridportable.hpp"
#include "gridgain/gridclienthash.hpp"
#include "gridgain/gridportablereader.hpp"
#include "gridgain/gridportablewriter.hpp"
#include "gridgain/impl/utils/gridclientbyteutils.hpp"
#include "gridgain/impl/marshaller/portable/gridclientportablemessages.hpp"
#include "gridgain/impl/cmd/gridclientmessagetopologyresult.hpp"
#include "gridgain/impl/cmd/gridclientmessagelogresult.hpp"
#include "gridgain/impl/cmd/gridclientmessagecacheresult.hpp"
#include "gridgain/impl/cmd/gridclientmessagecachemodifyresult.hpp"
#include "gridgain/impl/cmd/gridclientmessagecachemetricsresult.hpp"
#include "gridgain/impl/cmd/gridclientmessagecachegetresult.hpp"
#include "gridgain/impl/cmd/gridclientmessagetaskresult.hpp"

const int8_t TYPE_ID_BYTE = 1;
const int8_t TYPE_ID_SHORT = 2;
const int8_t TYPE_ID_INT = 3;
const int8_t TYPE_ID_LONG = 4;
const int8_t TYPE_ID_FLOAT = 5;
const int8_t TYPE_ID_DOUBLE = 6;
const int8_t TYPE_ID_CHAR = 7;
const int8_t TYPE_ID_BOOLEAN = 8;

const int8_t TYPE_ID_STRING = 9;
const int8_t TYPE_ID_UUID = 10;

const int8_t TYPE_ID_BYTE_ARR = 11;
const int8_t TYPE_ID_SHORT_ARR = 12;
const int8_t TYPE_ID_INT_ARR = 13;
const int8_t TYPE_ID_LONG_ARR = 14;
const int8_t TYPE_ID_FLOAT_ARR = 15;
const int8_t TYPE_ID_DOUBLE_ARR = 16;
const int8_t TYPE_ID_CHAR_ARR = 17;
const int8_t TYPE_ID_BOOLEAN_ARR = 18;

const int8_t TYPE_ID_STRING_ARR = 19;
const int8_t TYPE_ID_UUID_ARR = 20;
const int8_t TYPE_ID_OBJ_ARR = 21;

const int8_t TYPE_ID_COLLECTION = 100;
const int8_t TYPE_ID_MAP = 200;

const int8_t FLAG_NULL = 0x80;
const int8_t FLAG_HANDLE = 0x81;
const int8_t FLAG_OBJECT = 0x82;
const int8_t FLAG_METADATA = 0x83;

GridPortable* createPortable(int32_t typeId, GridPortableReader &reader);

int32_t cStringHash(const char* str);

class GridWriteHandleTable {
public:    
    GridWriteHandleTable(int initCap, float loadFactor) : spine(initCap, -1), next(initCap, 0), objs(initCap, nullptr), size(0) {
        this->loadFactor = loadFactor;

        threshold = (int)(initCap * loadFactor);
    }

    void clear() {
        std::fill(spine.begin(), spine.end(), -1); 
        
        std::fill(objs.begin(), objs.end(), nullptr); 

        size = 0;
    }

    int32_t lookup(void* obj) {
        int idx = hash(obj) % spine.size();

        if (size == 0) {
            assign(obj, idx);

            return -1;
        }

        for (int32_t i = spine[idx]; i >= 0; i = next[i]) {
            if (objs[i] == obj)
                return i;
        }

        assign(obj, idx);

        return -1;
    }

private:
    int hash(void* ptr) {
        return (size_t)ptr & 0x7FFFFFFF;            
    }

    void assign(void* obj, int idx) {
        if (size >= next.size())
            growEntries();

        if (size >= threshold) {
            growSpine();
        
            idx = hash(obj) % spine.size();
        }

        insert(obj, size, idx);

        size++;
    }

    void growEntries() {
        int newLen = (next.size() << 1) + 1;

        objs.resize(newLen);
        
        next.resize(newLen);
    }

    void growSpine() {
        int newLen = (spine.size() << 1) + 1;

        threshold = (int)(newLen * loadFactor);
        
        std::fill(spine.begin(), spine.end(), -1); 
        
        spine.resize(newLen, -1);

        for (int i = 0; i < this->size; i++) {
            void* obj = objs[i];

            int idx = hash(obj) % spine.size();

            insert(objs[i], i, idx);
        }
    }

    void insert(void* obj, int handle, int idx) {
        objs[handle] = obj;
        next[handle] = spine[idx];
        spine[idx] = handle;
    }

    int size;

    int threshold;

    float loadFactor;

    std::vector<int> spine;

    std::vector<int> next;

    std::vector<void*> objs;
};

class GridReadHandleTable {
public:
    GridReadHandleTable(int cap) {
        handles.reserve(cap);
    }

    int32_t assign(void* obj) {
        int32_t res = handles.size();

        handles.push_back(obj);

        return res;
    }

    void* lookup(int32_t handle) {
        return handles[handle];
    }

private:
    std::vector<void*> handles;
};

#ifdef BOOST_BIG_ENDIAN
class PortableOutput {
public:
	PortableOutput(size_t cap) {
        bytes.reserve(cap);
	}

    virtual ~PortableOutput() {
    }

    void writeBool(bool val) {
        writeByte(val ? 1 : 0);
    }

    void writeByte(int8_t val) {
        bytes.push_back(val);
    }

	void writeBytes(const void* src, size_t size) {
        const int8_t* ptr = reinterpret_cast<const int8_t*>(src);

        bytes.insert(bytes.end(), ptr, ptr + size);
	}

    void writeInt16Array(const std::vector<int16_t>& val) {
        for (auto iter = val.begin(); iter != val.end(); ++iter)
            writeInt16(*iter);
    }

    void writeInt16Array(const int16_t* val, int32_t size) {
        for (int i = 0; i < size; i++)
            writeInt16(val[i]);
    }

    void writeInt32Array(const std::vector<int32_t>& val) {
        for (auto iter = val.begin(); iter != val.end(); ++iter)
            writeInt32(*iter);
    }

    void writeInt32Array(const int32_t* val, int32_t size) {
        for (int i = 0; i < size; i++)
            writeInt32(val[i]);
    }

    void writeCharArray(const std::vector<uint16_t>& val) {
        for (auto iter = val.begin(); iter != val.end(); ++iter)
            writeChar(*iter);
    }

    void writeCharArray(const uint16_t* val, int32_t size) {
        for (int i = 0; i < size; i++)
            writeChar(val[i]);
    }

    void writeInt64Array(const std::vector<int64_t>& val) {
        for (auto iter = val.begin(); iter != val.end(); ++iter)
            writeInt64(*iter);
    }

    void writeInt64Array(const int64_t* val, int32_t size) {
        for (int i = 0; i < size; i++)
            writeInt64(val[i]);
    }

    void writeFloatArray(const std::vector<float>& val) {
        for (auto iter = val.begin(); iter != val.end(); ++iter)
            writeFloat(*iter);
    }

    void writeFloatArray(const float* val, int32_t size) {
        for (int i = 0; i < size; i++)
            writeFloat(val[i]);
    }

    void writeDoubleArray(const std::vector<double>& val) {
        for (auto iter = val.begin(); iter != val.end(); ++iter)
            writeDouble(*iter);
    }

    void writeDoubleArray(const double* val, int32_t size) {
        for (int i = 0; i < size; i++)
            writeDouble(val[i]);
    }

    void writeBoolArray(const std::vector<bool>& val) {
        for (int i = 0; i < val.size(); i++)
            writeByte(val[i] ? 1 : 0);
    }

    void writeBoolArray(const bool* val, int32_t size) {
        for (int i = 0; i < size; i++)
            writeByte(val[i] ? 1 : 0);
    }

	void writeInt16(int16_t val) {
        int8_t* ptr = reinterpret_cast<int8_t*>(&val);

        size_t size = bytes.size();

        bytes.resize(size + 2);

        std::reverse_copy(ptr, ptr + 2, bytes.data() + size);
	}
    
    void writeChar(uint16_t val) {
        int8_t* ptr = reinterpret_cast<int8_t*>(&val);

        size_t size = bytes.size();

        bytes.resize(size + 2);

        std::reverse_copy(ptr, ptr + 2, bytes.data() + size);
    }

	void writeInt32(int32_t val) {
        int8_t* ptr = reinterpret_cast<int8_t*>(&val);

        size_t size = bytes.size();

        bytes.resize(size + 4);

        std::reverse_copy(ptr, ptr + 4, bytes.data() + size);
	}

    void writeInt32(int32_t val, int32_t pos) {
        assert(pos < bytes.size());

        int8_t* ptr = reinterpret_cast<int8_t*>(&val);
        
        int8_t* dst = reinterpret_cast<int8_t*>(bytes.data());

        std::reverse_copy(ptr, ptr + 4, dst + pos);
    }

    void writeInt64(int64_t val) {
        int8_t* ptr = reinterpret_cast<int8_t*>(&val);

        size_t size = bytes.size();

        bytes.resize(size + 8);

        std::reverse_copy(ptr, ptr + 8, bytes.data() + size);
	}

    void writeFloat(float val) {
        int8_t* ptr = reinterpret_cast<int8_t*>(&val);

        size_t size = bytes.size();

        bytes.resize(size + 4);

        std::reverse_copy(ptr, ptr + 4, bytes.data() + size);
	}

    void writeDouble(double val) {
        int8_t* ptr = reinterpret_cast<int8_t*>(&val);

        size_t size = bytes.size();

        bytes.resize(size + 8);

        std::reverse_copy(ptr, ptr + 8, bytes.data() + size);
	}

    std::vector<int8_t> bytes;
};
#else
class PortableOutput {
public:
	PortableOutput(size_t cap) {
        bytes.reserve(cap);
	}

    virtual ~PortableOutput() {
    }

    void writeBool(bool val) {
        writeByte(val ? 1 : 0);
    }

    void writeByte(int8_t val) {
        bytes.push_back(val);
    }

	void writeBytes(const void* src, size_t size) {
        const int8_t* ptr = reinterpret_cast<const int8_t*>(src);

        bytes.insert(bytes.end(), ptr, ptr + size);
	}

    void writeInt16Array(const std::vector<int16_t>& val) {
        writeBytes(val.data(), val.size() * 2);
    }

    void writeInt16Array(const int16_t* val, int32_t size) {
        writeBytes(val, size * 2);
    }

    void writeInt32Array(const std::vector<int32_t>& val) {
        writeBytes(val.data(), val.size() * 4);
    }

    void writeInt32Array(const int32_t* val, int32_t size) {
        writeBytes(val, size * 4);
    }

    void writeCharArray(const std::vector<uint16_t>& val) {
        writeBytes(val.data(), val.size() * 2);
    }

    void writeCharArray(const uint16_t* val, int32_t size) {
        writeBytes(val, size * 2);
    }

    void writeInt64Array(const std::vector<int64_t>& val) {
        writeBytes(val.data(), val.size() * 8);
    }

    void writeInt64Array(const int64_t* val, int32_t size) {
        writeBytes(val, size * 8);
    }

    void writeFloatArray(const std::vector<float>& val) {
        writeBytes(val.data(), val.size() * 4);
    }

    void writeFloatArray(const float* val, int32_t size) {
        writeBytes(val, size * 4);
    }

    void writeDoubleArray(const std::vector<double>& val) {
        writeBytes(val.data(), val.size() * 2);
    }

    void writeDoubleArray(const double* val, int32_t size) {
        writeBytes(val, size * 8);
    }

    void writeBoolArray(const std::vector<bool>& val) {
        for (int i = 0; i < val.size(); i++)
            writeByte(val[i] ? 1 : 0);
    }

    void writeBoolArray(const bool* val, int32_t size) {
        for (int i = 0; i < size; i++)
            writeByte(val[i] ? 1 : 0);
    }

	void writeInt16(int16_t val) {
        int8_t* ptr = reinterpret_cast<int8_t*>(&val);

        bytes.insert(bytes.end(), ptr, ptr + 2);
	}

    void writeChar(uint16_t val) {
        int8_t* ptr = reinterpret_cast<int8_t*>(&val);

        bytes.insert(bytes.end(), ptr, ptr + 2);
    }

    void writeInt32(int32_t pos, int32_t val) {
        assert(pos < bytes.size());

        int8_t* ptr = reinterpret_cast<int8_t*>(&val);
        
        int8_t* dst = reinterpret_cast<int8_t*>(bytes.data());

        std::copy(ptr, ptr + 4, dst + pos);
    }

	void writeInt32(int32_t val) {
        int8_t* ptr = reinterpret_cast<int8_t*>(&val);

        bytes.insert(bytes.end(), ptr, ptr + 4);
	}

    void writeInt64(int64_t val) {
        int8_t* ptr = reinterpret_cast<int8_t*>(&val);

        bytes.insert(bytes.end(), ptr, ptr + 8);
	}

    void writeFloat(float val) {
        int8_t* ptr = reinterpret_cast<int8_t*>(&val);

        bytes.insert(bytes.end(), ptr, ptr + 4);
	}

    void writeDouble(double val) {
        int8_t* ptr = reinterpret_cast<int8_t*>(&val);

        bytes.insert(bytes.end(), ptr, ptr + 8);
	}

    std::vector<int8_t> bytes;
};
#endif;

class WriteContext {
public:
    WriteContext(int cap, GridPortableIdResolver* idRslvr) : out(cap), handles(10, 3), idRslvr(idRslvr) {
    }

    GridPortableIdResolver* idRslvr;

    GridWriteHandleTable handles;

    PortableOutput out;
};

const int32_t TOTAL_LENGTH_POS = 10;
const int32_t RAW_DATA_OFF_POS = 14;

class GridPortableWriterImpl : public GridPortableWriter, public GridPortableRawWriter {
public:
	int32_t curTypeId;

    GridPortableWriterImpl(WriteContext& ctx, int32_t curTypeId) : ctx(ctx), start(ctx.out.bytes.size()), allowFields(true), curTypeId(curTypeId) {
    }

    void writePortable(GridPortable& portable) {
        doWriteVariant(GridClientVariant(&portable));
    }

    GridPortableRawWriter& rawWriter() override {
        return *this;
    }

    void writeFieldName(char* fieldName) {
        if (!allowFields)
            throw GridClientPortableException("Named fields are not allowed after raw data.");

        ctx.out.writeInt32(fieldId(fieldName));
    }

    int32_t fieldId(char* fieldName) {
        if (ctx.idRslvr != nullptr) {
            boost::optional<int32_t> id = ctx.idRslvr->fieldId(curTypeId, fieldName);

            if (id.is_initialized())
                return id.get();
        }

        /*
        std::string str(fieldName);

        boost::to_lower(str);

        return gridStringHash(str);
        */

        return cStringHash(fieldName); // TODO 8536
    }

    void switchToRaw() {
        if (allowFields) {
            ctx.out.writeInt32(ctx.out.bytes.size() - start, start + RAW_DATA_OFF_POS);

            allowFields = false;
        }
    }

    void writeByte(char* fieldName, int8_t val) override {
		writeFieldName(fieldName);

        ctx.out.writeInt32(2);
        ctx.out.writeByte(TYPE_ID_BYTE);

        doWriteByte(val);
	}

    void doWriteByte(int8_t val) {
        ctx.out.writeByte(val);
	}

    void writeInt16(char* fieldName, int16_t val) override {
		writeFieldName(fieldName);

        ctx.out.writeInt32(3);
        ctx.out.writeByte(TYPE_ID_SHORT);

        doWriteInt16(val);
	}

    void doWriteInt16(int16_t val) {
        ctx.out.writeInt16(val);
	}

    void writeChar(char* fieldName, uint16_t val) override {
		writeFieldName(fieldName);

        ctx.out.writeInt32(3);
        ctx.out.writeByte(TYPE_ID_CHAR);

        doWriteInt16(val);
    }

    void doWriteChar(uint16_t val) {
        ctx.out.writeChar(val);
	}
    
    void writeCharCollection(char* fieldName, const std::vector<uint16_t> val) override {
        writeCharArray(fieldName, val.data(), val.size());
    }

    void writeCharArray(char* fieldName, const uint16_t* val, int32_t size) override {
		writeFieldName(fieldName);

        if (val != nullptr) {
            ctx.out.writeInt32(5 + size * 2);
            ctx.out.writeByte(TYPE_ID_CHAR_ARR);

            ctx.out.writeInt32(size);
            ctx.out.writeCharArray(val, size);
        }
        else {
            ctx.out.writeInt32(5 + size * 2);
            ctx.out.writeByte(TYPE_ID_CHAR_ARR);

            ctx.out.writeInt32(-1);
        }
    }
	
    void writeInt16Collection(char* fieldName, const std::vector<int16_t>& val) override {
        writeInt16Array(fieldName, val.data(), val.size());
    }
    
    void writeInt16Array(char* fieldName, const int16_t* val, int32_t size) override {
		writeFieldName(fieldName);

        if (val != nullptr) {
            ctx.out.writeInt32(5 + size * 2);
            ctx.out.writeByte(TYPE_ID_SHORT_ARR);

            ctx.out.writeInt32(size);
            ctx.out.writeInt16Array(val, size);
        }
        else {
            ctx.out.writeInt32(5 + size * 2);
            ctx.out.writeByte(TYPE_ID_SHORT_ARR);

            ctx.out.writeInt32(-1);
        }
    }

    void writeInt32(char* fieldName, int32_t val) override {
		writeFieldName(fieldName);

        ctx.out.writeInt32(5);
        ctx.out.writeByte(TYPE_ID_INT);

        doWriteInt32(val);
	}

    void doWriteInt32(int32_t val) {
        ctx.out.writeInt32(val);
	}
	
    void writeInt32Collection(char* fieldName, const std::vector<int32_t>& val) override {
		writeFieldName(fieldName);

        writeInt32Array(val.data(), val.size());
    }
    
    void writeInt32Array(char* fieldName, const int32_t* val, int32_t size) override {
		writeFieldName(fieldName);

        if (val != nullptr) {
            ctx.out.writeInt32(5 + size * 4);
            ctx.out.writeByte(TYPE_ID_INT_ARR);

            ctx.out.writeInt32(size);
            ctx.out.writeInt32Array(val, size);
        }
        else {
            ctx.out.writeInt32(5 + size * 4);
            ctx.out.writeByte(TYPE_ID_INT_ARR);

            ctx.out.writeInt32(-1);
        }
    }

    void writeInt64(char* fieldName, int64_t val) override {
		writeFieldName(fieldName);

        ctx.out.writeInt32(9);
        ctx.out.writeByte(TYPE_ID_LONG);

        doWriteInt64(val);
	}

    void doWriteInt64(int64_t val) {
        ctx.out.writeInt64(val);
	}
	
    void writeInt64Collection(char* fieldName, const std::vector<int64_t>& val) override {
		writeFieldName(fieldName);

        writeInt64Array(fieldName, val.data(), val.size());
    }
    
    void writeInt64Array(char* fieldName, const int64_t* val, int32_t size) override {
		writeFieldName(fieldName);

        if (val != nullptr) {
            ctx.out.writeInt32(5 + size * 8);
            ctx.out.writeByte(TYPE_ID_LONG_ARR);

            ctx.out.writeInt32(size);
            ctx.out.writeInt64Array(val, size);
        }
        else {
            ctx.out.writeInt32(5 + size * 8);
            ctx.out.writeByte(TYPE_ID_LONG_ARR);

            ctx.out.writeInt32(-1);
        }
    }

    void writeFloat(char* fieldName, float val) override {
		writeFieldName(fieldName);

        ctx.out.writeInt32(5);
        ctx.out.writeByte(TYPE_ID_FLOAT);

        doWriteFloat(val);
	}

    void doWriteFloat(float val) {
        ctx.out.writeFloat(val);
	}
	
    void writeFloatCollection(char* fieldName, const std::vector<float>& val) override {
		writeFieldName(fieldName);

        writeFloatArray(fieldName, val.data(), val.size());
    }
    
    void writeFloatArray(char* fieldName, const float* val, int32_t size) override {
		writeFieldName(fieldName);

        if (val != nullptr) {
            ctx.out.writeInt32(5 + size * 4);
            ctx.out.writeByte(TYPE_ID_FLOAT_ARR);

            ctx.out.writeInt32(size);
            ctx.out.writeFloatArray(val, size);
        }
        else {
            ctx.out.writeInt32(5 + size * 4);
            ctx.out.writeByte(TYPE_ID_FLOAT_ARR);

            ctx.out.writeInt32(-1);
        }
    }

    void writeDouble(char* fieldName, double val) override {
		writeFieldName(fieldName);

        ctx.out.writeInt32(9);
        ctx.out.writeByte(TYPE_ID_DOUBLE);

        doWriteDouble(val);
	}

    void doWriteDouble(double val) {
        ctx.out.writeDouble(val);
	}
	
    void writeDoubleCollection(char* fieldName, const std::vector<double>& val) override {
		writeFieldName(fieldName);

        writeDoubleArray(fieldName, val.data(), val.size());
    }
    
    void writeDoubleArray(char* fieldName, const double* val, int32_t size) override {
		writeFieldName(fieldName);

        if (val != nullptr) {
            ctx.out.writeInt32(5 + size * 8);
            ctx.out.writeByte(TYPE_ID_DOUBLE_ARR);

            ctx.out.writeInt32(size);
            ctx.out.writeDoubleArray(val, size);
        }
        else {
            ctx.out.writeInt32(5 + size * 8);
            ctx.out.writeByte(TYPE_ID_DOUBLE_ARR);

            ctx.out.writeInt32(-1);
        }
    }

	void writeString(char* fieldName, const std::string &val) override {
		writeFieldName(fieldName);

        int32_t len = val.length() / sizeof(char);
        ctx.out.writeInt32(5 + len);

        ctx.out.writeByte(TYPE_ID_STRING);
        ctx.out.writeInt32(len);

		ctx.out.writeBytes(val.data(), len);
	}

    void doWriteString(const std::string &val) {
        int32_t len = val.length() / sizeof(char);
        
        ctx.out.writeInt32(len);
		ctx.out.writeBytes(val.data(), len);
	}

    void writeStringCollection(char* fieldName, const std::vector<std::string>& val) override {
		writeFieldName(fieldName);

        ctx.out.writeInt32(0);

        int32_t fieldStart = ctx.out.bytes.size();

        ctx.out.writeByte(TYPE_ID_STRING_ARR);

        doWriteStringCollection(val);

        int32_t len = ctx.out.bytes.size() - fieldStart;

        ctx.out.writeInt32(len, fieldStart - 4);
    }

    void doWriteStringCollection(const std::vector<std::string>& val) {
        ctx.out.writeInt32(val.size());

        for (auto iter = val.begin(); iter != val.end(); ++iter)
            doWriteString(*iter);
    }

    void writeWString(char* fieldName, const std::wstring& val) override {
		writeFieldName(fieldName);

        int32_t len = val.length() / sizeof(wchar_t);
        ctx.out.writeInt32(5 + len);

        ctx.out.writeByte(TYPE_ID_STRING);

		ctx.out.writeBytes(val.data(), len);
    }

    void doWriteWString(const std::wstring &str) {
        int32_t len = str.length() / sizeof(wchar_t);
        
		ctx.out.writeBytes(str.data(), len);
	}

    void writeWStringCollection(char* fieldName, const std::vector<std::wstring>& val) override {
		writeFieldName(fieldName);

        ctx.out.writeInt32(0);

        int32_t fieldStart = ctx.out.bytes.size();

        ctx.out.writeByte(TYPE_ID_STRING_ARR);

        doWriteWStringCollection(val);

        int32_t len = ctx.out.bytes.size() - fieldStart;

        ctx.out.writeInt32(len, fieldStart - 4);
    }

    void doWriteWStringCollection(const std::vector<std::wstring>& val) {
        ctx.out.writeInt32(val.size());

        for (auto iter = val.begin(); iter != val.end(); ++iter)
            doWriteWString(*iter);
    }
	
    void writeByteCollection(char* fieldName, const std::vector<int8_t>& val) override {
		writeFieldName(fieldName);

        ctx.out.writeInt32(5 + val.size());

        ctx.out.writeByte(TYPE_ID_BYTE_ARR);

        doWriteByteCollection(val);
    }

    void doWriteByteCollection(const std::vector<int8_t>& val) {
        ctx.out.writeBytes(val.data(), val.size());
    }
    
    void writeByteArray(char* fieldName, const int8_t* val, int32_t size) override {
		writeFieldName(fieldName);

        ctx.out.writeInt32(val != nullptr ? 5 + size : 5);

        ctx.out.writeByte(TYPE_ID_BYTE_ARR);

        doWriteByteArray(val, size);
    }
    
    void doWriteByteArray(const int8_t* val, int32_t size) {
        if (val != nullptr) {
            ctx.out.writeInt32(size);
            ctx.out.writeBytes(val, size);
        }
        else
            ctx.out.writeInt32(-1);
    }

	void writeBool(char* fieldName, bool val) override {
		writeFieldName(fieldName);

        ctx.out.writeInt32(2);
        ctx.out.writeByte(TYPE_ID_BOOLEAN);

        doWriteBool(val);
	}

	void doWriteBool(bool val) {
        ctx.out.writeBool(val);
	}
	
    void writeBoolCollection(char* fieldName, const std::vector<bool>& val) override {
		writeFieldName(fieldName);

        ctx.out.writeInt32(val.size() + 1);
        ctx.out.writeByte(TYPE_ID_BOOLEAN_ARR);

        for (auto iter = val.begin(); iter != val.end(); ++iter)
            doWriteBool(*iter);
    }
    
    void writeBoolArray(char* fieldName, const bool* val, int32_t size) override {
		writeFieldName(fieldName);

        ctx.out.writeInt32(size + 1);
        ctx.out.writeByte(TYPE_ID_BOOLEAN_ARR);

        for (int32_t i = 0; i < size; i++)
            doWriteBool(val[i]);
    }

	void writeUuid(char* fieldName, const boost::optional<GridClientUuid>& val) override {
		writeFieldName(fieldName);

        ctx.out.writeInt32(val.is_initialized() ? 18 : 2);
    
        ctx.out.writeByte(TYPE_ID_UUID);

        doWriteUuid(val);
	}

    void doWriteUuid(const boost::optional<GridClientUuid>& val) {
        if (val) {
            ctx.out.writeBool(true);

            ctx.out.writeInt64(val.get().mostSignificantBits());
            ctx.out.writeInt64(val.get().leastSignificantBits());
        }
        else
            ctx.out.writeBool(false);
	}

    void writeUuidCollection(char* fieldName, const std::vector<GridClientUuid>& val) override {
		writeFieldName(fieldName);

        ctx.out.writeInt32(5 + val.size() * 16);
    
        ctx.out.writeByte(TYPE_ID_UUID);

        doWriteUuidCollection(val);
    }

    void doWriteUuidCollection(const std::vector<GridClientUuid>& val) {
        ctx.out.writeInt32(val.size());

        for (auto iter = val.begin(); iter != val.end(); ++iter) {
            GridClientUuid uuid = *iter;

            ctx.out.writeBool(true);

            ctx.out.writeInt64(uuid.mostSignificantBits());
            ctx.out.writeInt64(uuid.leastSignificantBits());
        }
    }

    int32_t writeHeader(bool userType, int32_t typeId, const GridClientVariant& val) {
        ctx.out.writeBool(userType);
        ctx.out.writeInt32(typeId);
        ctx.out.writeInt32(val.hasPortable() ? 0 : val.hashCode());
        
        int32_t lenPos = ctx.out.bytes.size();

        ctx.out.writeInt64(0); // Reserve space for length and raw data offset.

        return lenPos;
    }

    void writeVariant(char* fieldName, const GridClientVariant& val) override {
		writeFieldName(fieldName);

        ctx.out.writeInt32(0); // Reserve space for length.

        int32_t fieldStart = ctx.out.bytes.size();

        doWriteVariant(val);

        int32_t len = ctx.out.bytes.size() - fieldStart;

        ctx.out.writeInt32(len, fieldStart - 4);
    }

    void doWriteVariant(const GridClientVariant &val) {
        int32_t start = ctx.out.bytes.size();
        int32_t lenPos = start + 10;

        if (val.hasPortable() || val.hasHashablePortable()) {
            GridPortable* portable = val.hasPortable() ? val.getPortable() : val.getHashablePortable();

            int32_t handle = ctx.handles.lookup(portable);

            if (handle >= 0) {
                ctx.out.writeByte(FLAG_HANDLE);
    
                ctx.out.writeInt32(handle);

                return;
            }
            else {
                ctx.out.writeByte(FLAG_OBJECT);

                lenPos = writeHeader(true, portable->typeId(), val);

                GridPortableWriterImpl writer(ctx, portable->typeId());

                portable->writePortable(writer);
            }
        }
        else if (val.hasPortableObject()) {
            // TODO
        }
        else if (val.hasByte()) {
            ctx.out.writeByte(FLAG_OBJECT);

            lenPos = writeHeader(false, TYPE_ID_BOOLEAN, val);

            doWriteByte(val.getByte());
        }
        else if (val.hasShort()) {
            ctx.out.writeByte(FLAG_OBJECT);

            lenPos = writeHeader(false, TYPE_ID_SHORT, val);

            doWriteInt16(val.getShort());
        }
        else if (val.hasInt()) {
            ctx.out.writeByte(FLAG_OBJECT);

            lenPos = writeHeader(false, TYPE_ID_INT, val);

            doWriteInt32(val.getInt());
        }
        else if (val.hasLong()) {
            ctx.out.writeByte(FLAG_OBJECT);

            lenPos = writeHeader(false, TYPE_ID_LONG, val);

            doWriteInt64(val.getLong());
        }
        else if (val.hasFloat()) {
            ctx.out.writeByte(FLAG_OBJECT);

            lenPos = writeHeader(false, TYPE_ID_FLOAT, val);

            doWriteFloat(val.getFloat());
        }
        else if (val.hasDouble()) {
            ctx.out.writeByte(FLAG_OBJECT);

            lenPos = writeHeader(false, TYPE_ID_DOUBLE, val);

            doWriteDouble(val.getInt());
        }
        else if (val.hasChar()) {
            ctx.out.writeByte(FLAG_OBJECT);

            lenPos = writeHeader(false, TYPE_ID_CHAR, val);

            doWriteChar(val.getChar());
        }
        else if (val.hasString()) {
            ctx.out.writeByte(FLAG_OBJECT);

            lenPos = writeHeader(false, TYPE_ID_STRING, val);

            doWriteString(val.getString());
        }
        else if (val.hasBool()) {
            ctx.out.writeByte(FLAG_OBJECT);

            lenPos = writeHeader(false, TYPE_ID_BOOLEAN, val);

            doWriteBool(val.getBool());
        }
        else if (val.hasUuid()) {
            ctx.out.writeByte(FLAG_OBJECT);

            lenPos = writeHeader(false, TYPE_ID_UUID, val);

            doWriteInt32(val.getInt());
        }
        else if (val.hasByteArray()) {
            ctx.out.writeByte(FLAG_OBJECT);

            lenPos = writeHeader(false, TYPE_ID_BYTE_ARR, val);

            std::vector<int8_t>& arr = val.getByteArray();

            doWriteByteArray(arr.data(), arr.size());
        }
        else if (val.hasShortArray()) {
            ctx.out.writeByte(FLAG_OBJECT);

            lenPos = writeHeader(false, TYPE_ID_SHORT_ARR, val);

            std::vector<int16_t>& arr = val.getShortArray();

            ctx.out.writeInt32(arr.size());
            ctx.out.writeInt16Array(arr.data(), arr.size());
        }
        else if (val.hasIntArray()) {
            ctx.out.writeByte(FLAG_OBJECT);

            lenPos = writeHeader(false, TYPE_ID_INT_ARR, val);

            std::vector<int32_t>& arr = val.getIntArray();

            ctx.out.writeInt32(arr.size());
            ctx.out.writeInt32Array(arr.data(), arr.size());
        }
        else if (val.hasLongArray()) {
            ctx.out.writeByte(FLAG_OBJECT);

            lenPos = writeHeader(false, TYPE_ID_LONG_ARR, val);

            std::vector<int64_t>& arr = val.getLongArray();

            ctx.out.writeInt32(arr.size());
            ctx.out.writeInt64Array(arr.data(), arr.size());
        }
        else if (val.hasFloatArray()) {
            ctx.out.writeByte(FLAG_OBJECT);

            lenPos = writeHeader(false, TYPE_ID_FLOAT_ARR, val);

            std::vector<float>& arr = val.getFloatArray();

            ctx.out.writeInt32(arr.size());
            ctx.out.writeFloatArray(arr.data(), arr.size());
        }
        else if (val.hasDoubleArray()) {
            ctx.out.writeByte(FLAG_OBJECT);

            lenPos = writeHeader(false, TYPE_ID_DOUBLE_ARR, val);

            std::vector<double>& arr = val.getDoubleArray();

            ctx.out.writeInt32(arr.size());
            ctx.out.writeDoubleArray(arr.data(), arr.size());
        }
        else if (val.hasCharArray()) {
            ctx.out.writeByte(FLAG_OBJECT);

            lenPos = writeHeader(false, TYPE_ID_CHAR_ARR, val);

            std::vector<uint16_t>& arr = val.getCharArray();

            ctx.out.writeInt32(arr.size());
            ctx.out.writeCharArray(arr.data(), arr.size());
        }
        else if (val.hasStringArray()) {
            ctx.out.writeByte(FLAG_OBJECT);

            lenPos = writeHeader(false, TYPE_ID_STRING_ARR, val);

            std::vector<std::string>& arr = val.getStringArray();

            doWriteStringCollection(arr);
        }
        else if (val.hasBoolArray()) {
            ctx.out.writeByte(FLAG_OBJECT);

            lenPos = writeHeader(false, TYPE_ID_BOOLEAN_ARR, val);

            std::vector<bool>& arr = val.getBoolArray();

            for (auto iter = arr.begin(); iter != arr.end(); ++iter)
                doWriteBool(*iter);
        }
        else if (val.hasUuidArray()) {
            ctx.out.writeByte(FLAG_OBJECT);

            lenPos = writeHeader(false, TYPE_ID_UUID_ARR, val);

            std::vector<GridClientUuid>& arr = val.getUuidArray();

            doWriteUuidCollection(arr);
        }
        else if (val.hasVariantVector()) {
            ctx.out.writeByte(FLAG_OBJECT);

            lenPos = writeHeader(false, TYPE_ID_COLLECTION, val);

            doWriteVariantCollection(val.getVariantVector());
        }
        else if (val.hasVariantMap()) {
            ctx.out.writeByte(FLAG_OBJECT);

            lenPos = writeHeader(false, TYPE_ID_MAP, val);

            doWriteVariantMap(val.getVariantMap());
        }
        else if (!val.hasAnyValue()) {
            ctx.out.writeByte(FLAG_NULL);

            return;
        }
        else {
            assert(false);

            throw GridClientPortableException("Unknown object type.");
        }

        assert(lenPos > 0);

        int32_t len = ctx.out.bytes.size() - start;

        ctx.out.writeInt32(lenPos, len);
    }

    void writeVariantCollection(char* fieldName, const TGridClientVariantSet &val) override {
        writeFieldName(fieldName);

        ctx.out.writeInt32(0); // Reserve space for length.

        int32_t fieldStart = ctx.out.bytes.size();

        doWriteVariantCollection(val);

        int32_t len = ctx.out.bytes.size() - fieldStart;

        ctx.out.writeInt32(len, fieldStart - 4);
    }

    void doWriteVariantCollection(const TGridClientVariantSet& col) {
        ctx.out.writeInt32(col.size());

        for (auto iter = col.begin(); iter != col.end(); ++iter) {
            GridClientVariant variant = *iter;

            doWriteVariant(variant);
        }
    }

    void writeVariantMap(char* fieldName, const TGridClientVariantMap &val) override {
        writeFieldName(fieldName);

        ctx.out.writeInt32(0); // Reserve space for length.

        int32_t fieldStart = ctx.out.bytes.size();

        doWriteVariantMap(val);

        int32_t len = ctx.out.bytes.size() - fieldStart;

        ctx.out.writeInt32(len, fieldStart - 4);
    }

    void doWriteVariantMap(const TGridClientVariantMap& map) {
        ctx.out.writeInt32(map.size());

        for (auto iter = map.begin(); iter != map.end(); ++iter) {
            GridClientVariant key = iter->first;
            GridClientVariant val = iter->second;

            doWriteVariant(key);
            doWriteVariant(val);
        }
    }

    void writeByte(int8_t val) override {
        switchToRaw();

        doWriteByte(val);
	}

    void writeInt16(int16_t val) override {
        switchToRaw();

        doWriteInt16(val);
	}
	
    void writeInt16Collection(const std::vector<int16_t>& val) override {
        writeInt16Array(val.data(), val.size());
    }

    void writeInt16Array(const int16_t* val, int32_t size) override {
        switchToRaw();

        if (val != nullptr) {
            ctx.out.writeInt32(size);
            ctx.out.writeInt16Array(val, size);
        }
        else
            ctx.out.writeInt32(-1);
    }

    void writeInt32(int32_t val) override {
        switchToRaw();

        doWriteInt32(val);
	}
	
    void writeInt32Collection(const std::vector<int32_t>& val) override {
        writeInt32Array(val.data(), val.size());
    }
    
    void writeInt32Array(const int32_t* val, int32_t size) override {
        switchToRaw();

        if (val != nullptr) {
            ctx.out.writeInt32(size);
            ctx.out.writeInt32Array(val, size);
        }
        else
            ctx.out.writeInt32(-1);
    }
    
    void writeCharCollection(const std::vector<uint16_t> val) override {
        writeCharArray(val.data(), val.size());
    }

    void writeCharArray(const uint16_t* val, int32_t size) override {
        switchToRaw();

        if (val != nullptr) {
            ctx.out.writeInt32(size);
            ctx.out.writeCharArray(val, size);
        }
        else
            ctx.out.writeInt32(-1);
    }

    void writeInt64(int64_t val) override {
        switchToRaw();

        doWriteInt64(val);
	}
	
    void writeInt64Collection(const std::vector<int64_t>& val) override {
        writeInt64Array(val.data(), val.size());
    }
    
    void writeInt64Array(const int64_t* val, int32_t size) override {
        switchToRaw();

        if (val != nullptr) {
            ctx.out.writeInt32(size);
            ctx.out.writeInt64Array(val, size);
        }
        else
            ctx.out.writeInt32(-1);
    }

    void writeFloat(float val) override {
        switchToRaw();

        doWriteFloat(val);
	}
	
    void writeFloatCollection(const std::vector<float>& val) override {
        writeFloatArray(val.data(), val.size());
    }
    
    void writeFloatArray(const float* val, int32_t size) override {
        switchToRaw();

        if (val != nullptr) {
            ctx.out.writeInt32(size);
            ctx.out.writeFloatArray(val, size);
        }
        else
            ctx.out.writeInt32(-1);
    }

    void writeDouble(double val) override {
        switchToRaw();

        doWriteDouble(val);
	}
	
    void writeDoubleCollection(const std::vector<double>& val) override {
        switchToRaw();

        writeDoubleArray(val.data(), val.size());
    }
    
    void writeDoubleArray(const double* val, int32_t size) override {
        switchToRaw();

        if (val != nullptr) {
            ctx.out.writeInt32(size);
            ctx.out.writeDoubleArray(val, size);
        }
        else
            ctx.out.writeInt32(-1);
    }

	void writeString(const std::string& val) override {
        switchToRaw();

        doWriteString(val);
	}

    void writeStringCollection(const std::vector<std::string>& val) override {
        switchToRaw();

        doWriteStringCollection(val);
    }

    void writeWString(const std::wstring& val) override {
        switchToRaw();

        doWriteWString(val);
    }

    void writeWStringCollection(const std::vector<std::wstring>& val) override {
        switchToRaw();

        doWriteWStringCollection(val);
    }
	
    void writeByteCollection(const std::vector<int8_t>& val) override {
        switchToRaw();

        doWriteByteCollection(val);
    }
    
    void writeByteArray(const int8_t* val, int32_t size) override {
        switchToRaw();

        doWriteByteArray(val, size);
    }

	void writeBool(bool val) override {
        switchToRaw();

        doWriteBool(val);
	}
	
    void writeBoolCollection(const std::vector<bool>& val) override {
        switchToRaw();

        ctx.out.writeInt32(val.size());

        for (auto iter = val.begin(); iter != val.end(); ++iter)
            doWriteBool(*iter);
    }
    
    void writeBoolArray(const bool* val, int32_t size) override {
        switchToRaw();

        for (int i = 0; i < size; i++)
            doWriteBool(val[i]);
    }

	void writeUuid(const boost::optional<GridClientUuid>& val) override {
        switchToRaw();

        doWriteUuid(val);
	}

    void writeUuidCollection(const std::vector<GridClientUuid>& val) override {
        switchToRaw();

        doWriteUuidCollection(val);
    }

    void writeVariant(const GridClientVariant& val) override {
        switchToRaw();

        doWriteVariant(val);
    }
    
    void writeVariantCollection(const TGridClientVariantSet& val) override {
        switchToRaw();

        doWriteVariantCollection(val);
    }

    void writeVariantMap(const TGridClientVariantMap& val) override {
        switchToRaw();

        doWriteVariantMap(val);
    }

private:
    int32_t start;

    bool allowFields;

    WriteContext& ctx;
};

#ifdef BOOST_BIG_ENDIAN
class PortableInput {
public:
    PortableInput(std::vector<int8_t>& data) : bytes(data), pos(0) {
    }

    virtual ~PortableInput() {
    }

    int8_t readByte() {
        checkAvailable(1);

        return bytes[pos++];
    }

    int8_t readByte(int32_t off) {
        checkAvailable(off, 1);

        return bytes[off];
    }

    bool readBool(int32_t off) {
        checkAvailable(off, 1);

        int8_t val = bytes[off];

        return val != 0;
    }

    int32_t readInt32(int32_t off) {
        checkAvailable(off, 4);

        int32_t res;

        int8_t* start = bytes.data() + off;
        int8_t* end = start + 4;

        int8_t* dst = reinterpret_cast<int8_t*>(&res);

        std::reverse_copy(start, end, dst);

        return res;
    }

    std::vector<int8_t> readBytes(int32_t size) {
        checkAvailable(size);

        std::vector<int8_t> vec;

        vec.insert(vec.end(), bytes.data() + pos, bytes.data() + pos + size);

        pos += size;

        return vec;
    }

    int8_t* readBytesArray(int32_t size) {
        checkAvailable(size);

        int8_t* res = new int8_t[size];

        memcpy(res, bytes.data() + pos, size);

        pos += size;

        return res;
    }

    int16_t readInt16() {
        checkAvailable(2);

        int16_t res;

        int8_t* start = bytes.data() + pos;
        int8_t* end = start + 2;

        int8_t* dst = reinterpret_cast<int8_t*>(&res);

        std::reverse_copy(start, end, dst);

        pos += 2;

        return res;
    }

    int32_t readInt32() {
        checkAvailable(4);

        int32_t res;

        int8_t* start = bytes.data() + pos;
        int8_t* end = start + 4;

        int8_t* dst = reinterpret_cast<int8_t*>(&res);

        std::reverse_copy(start, end, dst);

        pos += 4;

        return res;
    }

    int64_t readInt64() {
        checkAvailable(8);

        int64_t res;

        int8_t* start = bytes.data() + pos;
        int8_t* end = start + 8;

        int8_t* dst = reinterpret_cast<int8_t*>(&res);

        std::reverse_copy(start, end, dst);

        pos += 8;

        return res;
    }

    float readFloat() {
        checkAvailable(4);

        float res;

        int8_t* start = bytes.data() + pos;
        int8_t* end = start + 4;

        int8_t* dst = reinterpret_cast<int8_t*>(&res);

        std::reverse_copy(start, end, dst);

        pos += 4;

        return res;
    }

    double readDouble() {
        checkAvailable(4);

        double res;

        int8_t* start = bytes.data() + pos;
        int8_t* end = start + 8;

        int8_t* dst = reinterpret_cast<int8_t*>(&res);

        std::reverse_copy(start, end, dst);

        pos += 8;

        return res;
    }

protected:
    void checkAvailable(size_t cnt) {
        assert(pos + cnt <= bytes.size());
    }

    void checkAvailable(int32_t off, size_t cnt) {
        assert(off + pos + cnt <= bytes.size());
    }
    
    int32_t pos;

    std::vector<int8_t>& bytes;
};
#else
class PortableInput {
public:
    PortableInput(const std::vector<int8_t>& data) : bytes(data) {
    }

    virtual ~PortableInput() {
    }

    int8_t readByte(int32_t off) {
        checkAvailable(off, 1);

        return bytes[off];
    }

    bool readBool(int32_t off) {
        checkAvailable(off, 1);

        int8_t val = bytes[off];

        return val != 0;
    }

    int32_t readInt32(int32_t off) {
        checkAvailable(off, 4);

        int32_t res;

        const int8_t* start = bytes.data() + off;
        const int8_t* end = start + 4;

        int8_t* dst = reinterpret_cast<int8_t*>(&res);

        std::copy(start, end, dst);

        return res;
    }

    std::vector<int8_t>&& readByteCollection(int32_t off, int32_t size) {
        checkAvailable(off, size);

        std::vector<int8_t> vec(bytes.data() + off, bytes.data() + off + size);

        return std::move(vec);
    }

    int8_t* readByteArray(int32_t off, int32_t size) {
        checkAvailable(off, size);

        int8_t* res = new int8_t[size];

        std::copy(bytes.data() + off, bytes.data() + off + size, res);

        return res;
    }

    std::vector<int16_t>&& readInt16Collection(int32_t off, int32_t size) {
        checkAvailable(off, size);

        std::vector<int16_t> vec(bytes.data() + off, bytes.data() + off + size);

        return std::move(vec);
    }

    int16_t* readInt16Array(int32_t off, int32_t size) {
        checkAvailable(off, size);

        int16_t* res = new int16_t[size];

        std::copy(bytes.data() + off, bytes.data() + off + size, res);

        return res;
    }

    int16_t readInt16(int32_t off) {
        checkAvailable(off, 2);

        int16_t res;

        const int8_t* start = bytes.data() + off;
        const int8_t* end = start + 2;

        int8_t* dst = reinterpret_cast<int8_t*>(&res);

        std::copy(start, end, dst);

        return res;
    }

    uint16_t readChar(int32_t off) {
        checkAvailable(off, 2);

        uint16_t res;

        const int8_t* start = bytes.data() + off;
        const int8_t* end = start + 2;

        int8_t* dst = reinterpret_cast<int8_t*>(&res);

        std::copy(start, end, dst);

        return res;
    }

    int64_t readInt64(int32_t off) {
        checkAvailable(off, 8);

        int64_t res;

        const int8_t* start = bytes.data() + off;
        const int8_t* end = start + 8;

        int8_t* dst = reinterpret_cast<int8_t*>(&res);

        std::copy(start, end, dst);

        return res;
    }

    float readFloat(int32_t off) {
        checkAvailable(off, 4);

        float res;

        const int8_t* start = bytes.data() + off;
        const int8_t* end = start + 4;

        int8_t* dst = reinterpret_cast<int8_t*>(&res);

        std::copy(start, end, dst);

        return res;
    }

    double readDouble(int32_t off) {
        checkAvailable(off, 4);

        double res;

        const int8_t* start = bytes.data() + off;
        const int8_t* end = start + 8;

        int8_t* dst = reinterpret_cast<int8_t*>(&res);

        std::copy(start, end, dst);

        return res;
    }

    const std::vector<int8_t>& bytes;

protected:
    void checkAvailable(int32_t off, size_t cnt) {
        assert(off + cnt <= bytes.size());
    }
};
#endif;

class ReadContext {
public:
    ReadContext(const std::vector<int8_t>& data, int32_t handlesCap, GridPortableIdResolver* idRslvr) : in(data), handles(handlesCap), idRslvr(idRslvr) {
    }

    GridPortableIdResolver* idRslvr;

    GridReadHandleTable handles;

    PortableInput in;
};

class GridPortableReaderImpl : public GridPortableReader, public GridPortableRawReader {
public:
    int32_t rawOff;

    int32_t off;

    bool offInit;

    int32_t curTypeId;

    boost::unordered_map<int32_t, int32_t> fieldOffs;

    GridPortableReaderImpl(ReadContext& ctx) : ctx(ctx), off(0), rawOff(0), curTypeId(0), offInit(false) {
    }

    GridPortable* deserializePortable() {
        curTypeId = ctx.in.readInt32(2);

        rawOff = ctx.in.readInt32(14);

        GridPortable* portable = createPortable(curTypeId, *this);

        return portable;
    }

    GridClientVariant unmarshal(bool raw) {
        int8_t flag = doReadByte(raw);                
        
        switch(flag) {
            case FLAG_NULL: {
                return GridClientVariant();
            }

            case FLAG_HANDLE: {
                return GridClientVariant();
            }

            case FLAG_OBJECT: {
                bool userType = doReadBool(raw);
               
                int32_t typeId = doReadInt32(raw);  
                
                int32_t hashCode = doReadInt32(raw);  
                
                int32_t len = doReadInt32(raw);

                int32_t rawOff = doReadInt32(raw);

                if (userType) {
                    std::vector<int8_t> objData(ctx.in.bytes.begin(), ctx.in.bytes.begin() + len);

                    GridPortableObject obj(std::move(objData), ctx.idRslvr);

                    return GridClientVariant(std::move(obj));
                }
                else
                    return readStandard(typeId, raw);
            }
        }
    }

    GridClientVariant unmarshalFieldStr(const std::string& fieldName) {
        off = fieldOffsetStr(fieldName);

        return doUnmarshalField();
    }

    GridClientVariant unmarshalField(const char* fieldName) {
        off = fieldOffset(fieldName);

        return doUnmarshalField();
    }

    GridClientVariant doUnmarshalField() {
        if (off == -1)
            return GridClientVariant();

        int8_t flag = doReadByte(false);

        if (flag == FLAG_OBJECT) {
            bool userType = doReadBool(false);
        
            int32_t typeId = doReadInt32(false);  

            int32_t hashCode = doReadInt32(false);  
                
            int32_t len = doReadInt32(false);

            int32_t rawOff = doReadInt32(false);

            if (userType) {
                std::vector<int8_t> objData(ctx.in.bytes.begin() + off, ctx.in.bytes.begin() + off + len);

                GridPortableObject obj(std::move(objData), ctx.idRslvr);

                return GridClientVariant(std::move(obj));
            }
            else
                return readStandard(typeId, false);
        }
        else if (flag == FLAG_HANDLE) {
            return GridClientVariant();
        }
        else if (flag == FLAG_NULL)
            return GridClientVariant();

        return readStandard(flag, false);
    }

    GridClientVariant readStandard(int32_t typeId, bool raw) {
        switch (typeId) {
            case TYPE_ID_BYTE: {
                int8_t val = doReadByte(raw);

                return GridClientVariant(val);
            }

            case TYPE_ID_BOOLEAN: {
                bool val = doReadBool(raw);

                return GridClientVariant(val);
            }

            case TYPE_ID_SHORT: {
                int16_t val = doReadInt16(raw);

                return GridClientVariant(val);
            }

            case TYPE_ID_CHAR: {
                uint16_t val = doReadChar(raw);

                return GridClientVariant(val);
            }

            case TYPE_ID_INT: {
                int32_t val = doReadInt32(raw);

                return GridClientVariant(val);
            }

            case TYPE_ID_LONG: {
                int64_t val = doReadInt64(raw);

                return GridClientVariant(val);
            }

            case TYPE_ID_STRING: {
                boost::optional<std::string> val = doReadString(raw);

                if (val.is_initialized())
                    return GridClientVariant(val.get());
                else
                    return GridClientVariant();
            }
        }

        throw GridClientPortableException("Invalid flag value.");
    }

    GridClientVariant unmarshalVariant(int32_t typeId, bool raw, int32_t dataStart) {
    }

    void checkType(int8_t expFlag, int8_t flag) {
       // if (flag != expFlag) TODO 8536
        //    throw GridClientPortableException("Unexpected field type.");
    }

    int32_t fieldIdStr(const std::string& fieldName) {
        return gridStringHash(fieldName); // TODO
    }

    int32_t fieldId(const char* fieldName) {
        if (ctx.idRslvr != nullptr) {
            boost::optional<int32_t> rslvrId = ctx.idRslvr->fieldId(curTypeId, fieldName);

            if (rslvrId.is_initialized())
                return rslvrId.get();
        }
            
        return cStringHash(fieldName); // TODO
    }

    
    int32_t fieldOffsetStr(const std::string& fieldName) {
        int32_t id = fieldIdStr(fieldName);

        return fieldOffset(id);
    }
    
    int32_t fieldOffset(const char* fieldName) {
        int32_t id = fieldId(fieldName);

        return fieldOffset(id);
    }

    int32_t fieldOffset(int32_t id) {
        if (!offInit) {
            int32_t off = 18; // Header size.

            while(true) {
                if (off >= ctx.in.bytes.size())
                    break;

                int32_t id = ctx.in.readInt32(off);

                off += 4;

                int32_t len = ctx.in.readInt32(off);

                off += 4;

                fieldOffs[id] = off;

                off += len;
            }

            offInit = true;
        }

        boost::unordered_map<int32_t, int32_t>::const_iterator off = fieldOffs.find(id);

        if (off == fieldOffs.end())
            return -1;

        return (*off).second;
    }

    int8_t doReadByte(bool raw) {
        if (raw)
            return ctx.in.readByte(rawOff++);
        else
            return ctx.in.readByte(off++);
    }

    bool doReadBool(bool raw) {
        int8_t val;
        
        if (raw)
            val = ctx.in.readByte(rawOff++);
        else
            val = ctx.in.readByte(off++);

        return val != 0;
    }

    int16_t doReadInt16(bool raw) {
        int16_t res;

        if (raw) {
            res = ctx.in.readInt16(rawOff);

            rawOff += 2;
        }
        else {
            res = ctx.in.readInt16(off);

            off += 2;
        }

        return res;
    }

    uint16_t doReadChar(bool raw) {
        uint16_t res;

        if (raw) {
            res = ctx.in.readChar(rawOff);

            rawOff += 2;
        }
        else {
            res = ctx.in.readChar(off);

            off += 2;
        }

        return res;
    }

    int32_t doReadInt32(bool raw) {
        int32_t res;

        if (raw) {
            res = ctx.in.readInt32(rawOff);

            rawOff += 4;
        }
        else {
            res = ctx.in.readInt32(off);

            off += 4;
        }

        return res;
    }

    int64_t doReadInt64(bool raw) {
        int64_t res;

        if (raw) {
            res = ctx.in.readInt64(rawOff);

            rawOff += 4;
        }
        else {
            res = ctx.in.readInt64(off);

            off += 4;
        }

        return res;
    }

    float doReadFloat(bool raw) {
        float res;

        if (raw) {
            res = ctx.in.readFloat(rawOff);

            rawOff += 4;
        }
        else {
            res = ctx.in.readFloat(off);

            off += 4;
        }

        return res;
    }

    double doReadDouble(bool raw) {
        double res;

        if (raw) {
            res = ctx.in.readDouble(rawOff);

            rawOff += 2;
        }
        else {
            res = ctx.in.readDouble(off);

            off += 2;
        }

        return res;
    }

    GridPortableRawReader& rawReader() override {
        return *this;
    }

    int8_t readByte(char* fieldName) override {
        off = fieldOffset(fieldName);

        if (off >= 0) {
            int8_t flag = doReadByte(false);

            checkType(TYPE_ID_BYTE, flag);

            return doReadByte(false);
        }
        else
            return 0;
    }

    boost::optional<std::vector<int8_t>> readByteCollection(char* fieldName) override {
        boost::optional<std::vector<int8_t>> res;

        off = fieldOffset(fieldName);

        if (off >= 0) {
            int8_t flag = doReadByte(false);

            checkType(TYPE_ID_BYTE_ARR, flag);

            doReadByteCollection(false, res);
        }

        return res;
    }

    void doReadByteCollection(bool raw, boost::optional<std::vector<int8_t>>& res)  {
        int32_t len = doReadInt32(raw);

        if (len >= 0) {
            if (raw) {            
                res.reset(ctx.in.readByteCollection(rawOff, len));

                rawOff += len;
            }
            else {
                res.reset(ctx.in.readByteCollection(off, len));

                off += len;
            }
        }
    }

    std::pair<int8_t*, int32_t> readByteArray(char* fieldName) override {
        off = fieldOffset(fieldName);
        
        if (off >= 0) {
            int8_t flag = doReadByte(false);

            checkType(TYPE_ID_BYTE_ARR, flag);

            return doReadByteArray(false);
        }            

        return std::pair<int8_t*, int32_t>(nullptr, -1);
    }

    std::pair<int8_t*, int32_t> doReadByteArray(bool raw) {
        int32_t len = doReadInt32(raw);
        
        if (len >= 0) {
            if (raw) {            
                std::pair<int8_t*, int32_t> res(ctx.in.readByteArray(rawOff, len), len);
                
                rawOff += len;

                return res;
            }
            else {
                std::pair<int8_t*, int32_t> res(ctx.in.readByteArray(off, len), len);

                off += len;

                return res;
            }
        }
        else
            return std::pair<int8_t*, int32_t>(nullptr, 0);
    }

    int16_t readInt16(char* fieldName) override {
        off = fieldOffset(fieldName);

        if (off >= 0) {
            int8_t flag = doReadByte(false);

            checkType(TYPE_ID_SHORT, flag);

            return doReadInt16(false);
        }
        else
            return 0;
    }

    std::pair<int16_t*, int32_t> readInt16Array(char* fieldName) override {
        return std::pair<int16_t*, int32_t>(nullptr, -1); // TODO
    }

    std::pair<int16_t*, int32_t> doReadInt16Array(bool raw) {
        return std::pair<int16_t*, int32_t>(nullptr, -1); // TODO
    }

    boost::optional<std::vector<int16_t>> readInt16Collection(char* fieldName) override {
        return std::vector<int16_t>(); // TODO
    }

    void doReadInt16Collection(bool raw, boost::optional<std::vector<int16_t>>& res) {
    }

    int32_t readInt32(char* fieldName) override {
        off = fieldOffset(fieldName);

        if (off >= 0) {
            int8_t flag = doReadByte(false);

            checkType(TYPE_ID_INT, flag);

            return doReadInt32(false);
        }
        else
            return 0;
    }

    std::pair<int32_t*, int32_t> readInt32Array(char* fieldName) override {
        return std::pair<int32_t*, int32_t>(nullptr, -1); // TODO
    }

    std::pair<int32_t*, int32_t> doReadInt32Array(bool raw) {
        return std::pair<int32_t*, int32_t>(nullptr, -1); // TODO
    }

    boost::optional<std::vector<int32_t>> readInt32Collection(char* fieldName) override {
        return std::vector<int32_t>(); // TODO
    }

    void doReadInt32Collection(bool raw, boost::optional<std::vector<int32_t>>& res) {
        // TODO
    }

    int64_t readInt64(char* fieldName) override {
        off = fieldOffset(fieldName);

        if (off >= 0) {
            int8_t flag = doReadByte(false);

            checkType(TYPE_ID_LONG, flag);

            return doReadInt64(false);
        }
        else
            return 0;
    }

    std::pair<int64_t*, int32_t> readInt64Array(char* fieldName) override {
        return std::pair<int64_t*, int32_t>(nullptr, -1); // TODO
    }

    std::pair<int64_t*, int32_t> doReadInt64Array(bool raw) {
        return std::pair<int64_t*, int32_t>(nullptr, -1); // TODO
    }

    boost::optional<std::vector<int64_t>> readInt64Collection(char* fieldName) override {
        return std::vector<int64_t>(); // TODO
    }

    void doReadInt64Collection(bool raw, boost::optional<std::vector<int64_t>>& res) {
        // TODO
    }

    float readFloat(char* fieldName) override {
        off = fieldOffset(fieldName);

        if (off >= 0) {
            int8_t flag = doReadByte(false);

            checkType(TYPE_ID_FLOAT, flag);

            return doReadFloat(false);
        }
        else
            return 0;
    }

    std::pair<float*, int32_t> readFloatArray(char* fieldName) override {
        return std::pair<float*, int32_t>(nullptr, -1); // TODO
    }

    std::pair<float*, int32_t> doReadFloatArray(bool raw) {
        return std::pair<float*, int32_t>(nullptr, -1); // TODO
    }

    boost::optional<std::vector<float>> readFloatCollection(char* fieldName) override {
        return std::vector<float>(); // TODO
    }

    void doReadFloatCollection(bool raw, boost::optional<std::vector<float>>& res) {
        // TODO
    }

    double readDouble(char* fieldName) override {
        off = fieldOffset(fieldName);

        if (off >= 0) {
            int8_t flag = doReadByte(false);

            checkType(TYPE_ID_DOUBLE, flag);

            return doReadDouble(false);
        }
        else
            return 0;
    }

    std::pair<double*, int32_t> readDoubleArray(char* fieldName) override {
        return std::pair<double*, int32_t>(nullptr, -1); // TODO
    }

    std::pair<double*, int32_t> readDoubleArray(bool raw) {
        return std::pair<double*, int32_t>(nullptr, -1); // TODO
    }

    boost::optional<std::vector<double>> readDoubleCollection(char* fieldName) override {
        return std::vector<double>(); // TODO
    }

    void doReadDoubleCollection(bool raw, boost::optional<std::vector<double>>& res) {
        // TODO
    }

    boost::optional<std::string> readString(char* fieldName) override {
        off = fieldOffset(fieldName);

        if (off >= 0) {
            int8_t flag = doReadByte(false);

            checkType(TYPE_ID_STRING, flag);

            return doReadString(false);
        }
        else
            return 0;
    }

    boost::optional<std::string> doReadString(bool raw) {
        boost::optional<std::string> res;

        std::pair<int8_t*, int32_t> data = doReadByteArray(raw);

        if (data.first != nullptr) {
            res.reset(std::string(reinterpret_cast<char*>(data.first), data.second / sizeof(char)));

            delete[] data.first;
        }

        return res;
    }

    boost::optional<std::vector<std::string>> readStringCollection(char* fieldName) override {
        return std::vector<std::string>(); // TODO
    }

    void doReadStringCollection(bool raw, boost::optional<std::vector<std::string>>& res)  {
        // TODO
    }

    boost::optional<std::wstring> readWString(char* fieldName) override {
        return std::wstring(); // TODO
    }

    boost::optional<std::vector<std::wstring>> readWStringCollection(char* fieldName) override {
        return std::vector<std::wstring>(); // TODO
    }

    void doReadWStringCollection(bool raw, boost::optional<std::vector<std::wstring>>& res) {
        // TODO
    }

    bool readBool(char* fieldName) override {
        off = fieldOffset(fieldName);

        if (off >= 0) {
            int8_t flag = doReadByte(false);

            checkType(TYPE_ID_BOOLEAN, flag);

            return doReadBool(false);
        }
        else
            return 0;
    }

    boost::optional<std::vector<bool>> readBoolCollection(char* fieldName) override {
        return std::vector<bool>(); // TODO
    }

    void doReadBoolCollection(bool raw, boost::optional<std::vector<bool>>& res) {
        // TODO
    }

    std::pair<bool*, int32_t> readBoolArray(char* fieldName) override {
        return std::pair<bool*, int32_t>(nullptr, -1); // TODO
    }

    std::pair<bool*, int32_t> doReadBoolArray(bool raw)  {
        return std::pair<bool*, int32_t>(nullptr, -1); // TODO
    }

    boost::optional<GridClientUuid> readUuid(char* fieldName) override {
        off = fieldOffset(fieldName);

        if (off >= 0) {
            int8_t flag = doReadByte(false);

            checkType(TYPE_ID_UUID, flag);

            return doReadUuid(false);
        }
        else
            return 0;
    }

    boost::optional<GridClientUuid> doReadUuid(bool raw) {
        boost::optional<GridClientUuid> res;

        if (doReadBool(raw)) {
            int64_t most = doReadInt64(raw);
            int64_t least = doReadInt64(raw);

            res.reset(GridClientUuid(most, least));
        }

        return res;
    }

    GridClientVariant readVariant(char* fieldName) override {
        off = fieldOffset(fieldName);

        if (off >= 0)
            return unmarshal(false);

        return GridClientVariant();
    }

    boost::optional<TGridClientVariantSet> readVariantCollection(char* fieldName) override {
        boost::optional<TGridClientVariantSet> res;

        doReadVariantCollection(false, res);

        return res;
    }

    boost::optional<TGridClientVariantMap> readVariantMap(char* fieldName) override {
        boost::optional<TGridClientVariantMap> res;

        doReadVariantMap(false, res);

        return res;
    }

    int8_t readByte() override {
        return doReadByte(true);
    }

    boost::optional<std::vector<int8_t>> readByteCollection() override {
        boost::optional<std::vector<int8_t>> res;

        doReadByteCollection(true, res);

        return res;
    }

    std::pair<int8_t*, int32_t> readByteArray() override {
        return doReadByteArray(true);
    }

    int16_t readInt16() override {
        return doReadInt16(true);
    }

    std::pair<int16_t*, int32_t> readInt16Array() override {
        return doReadInt16Array(true);
    }

    boost::optional<std::vector<int16_t>> readInt16Collection() override {
        boost::optional<std::vector<int16_t>> res;

        doReadInt16Collection(true, res);

        return res;
    }

    int32_t readInt32() override {
        return doReadInt32(true);
    }

    std::pair<int32_t*, int32_t> readInt32Array() override {
        return doReadInt32Array(true);
    }

    boost::optional<std::vector<int32_t>> readInt32Collection() override {
        boost::optional<std::vector<int32_t>> res;

        doReadInt32Collection(true, res);

        return res;
    }

    int64_t readInt64() override {
        return doReadInt64(true);
    }

    std::pair<int64_t*, int32_t> readInt64Array() override {
        return doReadInt64Array(true);
    }

    boost::optional<std::vector<int64_t>> readInt64Collection() override {
        boost::optional<std::vector<int64_t>> res;

        doReadInt64Collection(true, res);

        return res;
    }

    float readFloat() override {
        return doReadFloat(true);
    }

    std::pair<float*, int32_t> readFloatArray() override {
        return doReadFloatArray(true);
    }

    boost::optional<std::vector<float>> readFloatCollection() override {
        boost::optional<std::vector<float>> res;

        doReadFloatCollection(true, res);

        return res;
    }

    double readDouble() override {
        return doReadDouble(true);
    }

    std::pair<double*, int32_t> readDoubleArray() override {
        return std::pair<double*, int32_t>(nullptr, -1); // TODO
    }

    boost::optional<std::vector<double>> readDoubleCollection() override {
        return std::vector<double>(); // TODO
    }

    boost::optional<std::string> readString() override { 
        return doReadString(true);
    }

    boost::optional<std::vector<std::string>> readStringCollection() override {
        return std::vector<std::string>(); // TODO
    }

    boost::optional<std::wstring> readWString() override {
        return std::wstring(); // TODO
    }

    boost::optional<std::vector<std::wstring>> readWStringCollection() override {
        return std::vector<std::wstring>(); // TODO
    }

    bool readBool() override {
        return doReadBool(true);
    }

    boost::optional<std::vector<bool>> readBoolCollection() override {
        return std::vector<bool>(); // TODO
    }

    std::pair<bool*, int32_t> readBoolArray() override {
        return std::pair<bool*, int32_t>(nullptr, -1); // TODO
    }

    boost::optional<GridClientUuid> readUuid() override {
        return doReadUuid(true);
    }

    GridClientVariant readVariant() override {
        return unmarshal(true);
    }

    boost::optional<TGridClientVariantSet> readVariantCollection() override {
        boost::optional<TGridClientVariantSet> res;
        
        doReadVariantCollection(true, res);

        return res;
    }

    boost::optional<TGridClientVariantMap> readVariantMap() override {
        boost::optional<TGridClientVariantMap> res;

        doReadVariantMap(true, res);

        return res;
    }

private:
    void doReadVariantCollection(bool raw, boost::optional<TGridClientVariantSet>& res) {
        int32_t size = doReadInt32(raw);

        if (size == -1)
            return;

        std::vector<GridClientVariant> vec;

        for (int i = 0; i < size; i++)
            vec.push_back(unmarshal(raw));

        res.reset(std::move(vec));
    }

    void doReadVariantMap(bool raw, boost::optional<TGridClientVariantMap>& res) {
        int32_t size = doReadInt32(raw);

        if (size == -1)
            return;
        
        boost::unordered_map<GridClientVariant, GridClientVariant> map;

        for (int i = 0; i < size; i++) {
            GridClientVariant key = unmarshal(raw);

            GridClientVariant val = unmarshal(raw);

            map[key] = val;
        }

        res.reset(std::move(map));
    }
    
    ReadContext& ctx;
};

class GridPortableMarshaller {
public:
    GridPortableIdResolver* idRslvr;

    GridPortableMarshaller() : idRslvr(nullptr) {
    }
    
    GridPortableMarshaller(GridPortableIdResolver* idRslvr) : idRslvr(idRslvr) {
    }
    
    std::vector<int8_t> marshal(GridClientVariant var) {
        WriteContext ctx(1024, idRslvr);

        GridPortableWriterImpl writer(ctx, 0);

		writer.doWriteVariant(var);

		return std::move(ctx.out.bytes);
    }

    std::vector<int8_t> marshal(GridPortable& portable) {
        WriteContext ctx(1024, idRslvr);

        GridPortableWriterImpl writer(ctx, portable.typeId());

		writer.writePortable(portable);

		return std::move(ctx.out.bytes);
	}

    GridClientVariant unmarshal(std::vector<int8_t>& data) {
		ReadContext ctx(data, 10, idRslvr);

        GridPortableReaderImpl reader(ctx);

        return reader.unmarshal(false);
    }

    template<typename T>
    T* unmarshalUserObject(std::vector<int8_t>& data) {
        GridClientVariant var = unmarshal(data);

        GridPortableObject& portable = var.getPortableObject();

        return portable.deserialize<T>();
    }

    void parseResponse(GridClientResponse* msg, GridClientMessageTopologyResult& resp) {
        GridClientVariant res = msg->getResult();

        assert(res.hasVariantVector());

        std::vector<GridClientVariant> vec = res.getVariantVector();

        std::vector<GridClientNode> nodes;

        for (auto iter = vec.begin(); iter != vec.end(); ++iter) {
            GridClientVariant nodeVariant = *iter;

            GridClientNodeBean* nodeBean = nodeVariant.getPortable<GridClientNodeBean>();
        
            nodes.push_back(nodeBean->createNode());

            delete nodeBean;
        }

        resp.setNodes(nodes);
    }

    void parseResponse(GridClientResponse* msg, GridClientMessageLogResult& resp) {
        GridClientVariant res = msg->getResult();

        assert(res.hasVariantVector());

        std::vector<GridClientVariant> vec = res.getVariantVector();

        std::vector<std::string> lines;
        
        for (auto iter = vec.begin(); iter != vec.end(); ++iter) {
            GridClientVariant line = *iter;

            assert(line.hasString());

            lines.push_back(line.getString());
        }

        resp.lines(lines);
    }

    void parseResponse(GridClientResponse* msg, GridClientMessageCacheGetResult& resp) {
        GridClientVariant res = msg->getResult();

        if (res.hasVariantMap()) {
            TCacheValuesMap keyValues = res.getVariantMap();
            
            resp.setCacheValues(keyValues);
        }
        else {
            TCacheValuesMap keyValues;

            keyValues.insert(std::make_pair(GridClientVariant(), res));

            resp.setCacheValues(keyValues);
        }
    }

    void parseResponse(GridClientResponse* msg, GridClientMessageCacheModifyResult& resp) {
        GridClientVariant res = msg->getResult();

        if (res.hasBool())
            resp.setOperationResult(res.getBool());
        else
            resp.setOperationResult(false);
    }

    void parseResponse(GridClientResponse* msg, GridClientMessageCacheMetricResult& resp) {
        GridClientVariant res = msg->getResult();

        assert(res.hasPortable());

        std::unique_ptr<GridClientCacheMetricsBean> metricsBean(res.getPortable<GridClientCacheMetricsBean>());

        TCacheMetrics metrics;

        metrics["createTime"] = (*metricsBean).createTime;
        metrics["readTime"] = (*metricsBean).readTime;
        metrics["writeTime"] = (*metricsBean).writeTime;
        metrics["reads"] = (*metricsBean).reads;
        metrics["writes"] = (*metricsBean).writes;
        metrics["hits"] = (*metricsBean).hits;
        metrics["misses"] = (*metricsBean).misses;

        resp.setCacheMetrics(metrics);
    }
    
    void parseResponse(GridClientResponse* msg, GridClientMessageTaskResult& resp) {
        GridClientVariant res = msg->getResult();

        if (res.hasPortable()) {
            assert(res.getPortable()->typeId() == -8);

            GridClientTaskResultBean* resBean = res.getPortable<GridClientTaskResultBean>();

            resp.setTaskResult(resBean->res);

            delete resBean;
        }
    }

};

#endif
