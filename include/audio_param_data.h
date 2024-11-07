#pragma once
#ifndef AUDIO_PARAM_DATA_H
#define AUDIO_PARAM_DATA_H

#include <cstddef> // Add this line to include size_t

class ParamData {
public:
    virtual ~ParamData() = default;
    virtual void * get_data() const = 0;
    virtual size_t get_size() const = 0;
};

class ParamFloatArrayData : public ParamData {
public:
    ParamFloatArrayData(unsigned int size)
            : m_data(new float[size]()),
              m_size(size) {}
    ~ParamFloatArrayData() override { delete[] m_data; }
    void * get_data() const override { return m_data; }
    size_t get_size() const override { return sizeof(float) * m_size; }
private:
    float * m_data;
    int m_size;
};

class ParamIntData : public ParamData {
public:
    ParamIntData()
            : m_data(0) {}
    ~ParamIntData() override = default;
    void * get_data() const override { return (void *) &m_data; }
    size_t get_size() const override { return sizeof(int); }
private:
    int m_data;
};

class ParamFloatData : public ParamData {
public:
    ParamFloatData()
            : m_data(0.0f) {}
    ~ParamFloatData() override = default;
    void * get_data() const override { return (void *) &m_data; }
    size_t get_size() const override { return sizeof(float); }
private:
    float m_data;
};

class ParamBoolData : public ParamData {
public:
    ParamBoolData()
            : m_data(false) {}
    ~ParamBoolData() override = default;
    void * get_data() const override { return (void *) &m_data; }
    size_t get_size() const override { return sizeof(bool); }
private:
    bool m_data;
};

#endif // AUDIO_PARAM_DATA_H