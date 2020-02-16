/*
 *  Copyright (C) 2015, Mike Walters <mike@flomp.net>
 *  Copyright (C) 2015, Jared Boone <jared@sharebrained.com>
 *
 *  This file is part of inspectrum.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "inputsource.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <stdexcept>
#include <algorithm>

#include <QFileInfo>

#include <sigmf/sigmf.h>

#include <QDebug>
#include <QElapsedTimer>
#include <QPainter>
#include <QPaintEvent>
#include <QPixmapCache>
#include <QRect>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>


class ComplexF32SampleAdapter : public SampleAdapter {
public:
    size_t sampleSize() override {
        return sizeof(std::complex<float>);
    }

    void copyRange(const void* const src, size_t start, size_t length, std::complex<float>* const dest) override {
        auto s = reinterpret_cast<const std::complex<float>*>(src);
        std::copy(&s[start], &s[start + length], dest);
    }
};

class ComplexS16SampleAdapter : public SampleAdapter {
public:
    size_t sampleSize() override {
        return sizeof(std::complex<int16_t>);
    }

    void copyRange(const void* const src, size_t start, size_t length, std::complex<float>* const dest) override {
        auto s = reinterpret_cast<const std::complex<int16_t>*>(src);
        std::transform(&s[start], &s[start + length], dest,
            [](const std::complex<int16_t>& v) -> std::complex<float> {
                const float k = 1.0f / 32768.0f;
                return { v.real() * k, v.imag() * k };
            }
        );
    }
};

class ComplexS8SampleAdapter : public SampleAdapter {
public:
    size_t sampleSize() override {
        return sizeof(std::complex<int8_t>);
    }

    void copyRange(const void* const src, size_t start, size_t length, std::complex<float>* const dest) override {
        auto s = reinterpret_cast<const std::complex<int8_t>*>(src);
        std::transform(&s[start], &s[start + length], dest,
            [](const std::complex<int8_t>& v) -> std::complex<float> {
                const float k = 1.0f / 128.0f;
                return { v.real() * k, v.imag() * k };
            }
        );
    }
};

class ComplexU8SampleAdapter : public SampleAdapter {
public:
    size_t sampleSize() override {
        return sizeof(std::complex<uint8_t>);
    }

    void copyRange(const void* const src, size_t start, size_t length, std::complex<float>* const dest) override {
        auto s = reinterpret_cast<const std::complex<uint8_t>*>(src);
        std::transform(&s[start], &s[start + length], dest,
            [](const std::complex<uint8_t>& v) -> std::complex<float> {
                const float k = 1.0f / 128.0f;
                return { (v.real() - 127.4f) * k, (v.imag() - 127.4f) * k };
            }
        );
    }
};

class RealF32SampleAdapter : public SampleAdapter {
public:
    size_t sampleSize() override {
        return sizeof(float);
    }

    void copyRange(const void* const src, size_t start, size_t length, std::complex<float>* const dest) override {
        auto s = reinterpret_cast<const float*>(src);
        std::transform(&s[start], &s[start + length], dest,
            [](const float& v) -> std::complex<float> {
                return {v, 0.0f};
            }
        );
    }
};

class RealS16SampleAdapter : public SampleAdapter {
public:
    size_t sampleSize() override {
        return sizeof(int16_t);
    }

    void copyRange(const void* const src, size_t start, size_t length, std::complex<float>* const dest) override {
        auto s = reinterpret_cast<const int16_t*>(src);
        std::transform(&s[start], &s[start + length], dest,
            [](const int16_t& v) -> std::complex<float> {
                const float k = 1.0f / 32768.0f;
                return { v * k, 0.0f };
            }
        );
    }
};

class RealS8SampleAdapter : public SampleAdapter {
public:
    size_t sampleSize() override {
        return sizeof(int8_t);
    }

    void copyRange(const void* const src, size_t start, size_t length, std::complex<float>* const dest) override {
        auto s = reinterpret_cast<const int8_t*>(src);
        std::transform(&s[start], &s[start + length], dest,
            [](const int8_t& v) -> std::complex<float> {
                const float k = 1.0f / 128.0f;
                return { v * k, 0.0f };
            }
        );
    }
};

class RealU8SampleAdapter : public SampleAdapter {
public:
    size_t sampleSize() override {
        return sizeof(uint8_t);
    }

    void copyRange(const void* const src, size_t start, size_t length, std::complex<float>* const dest) override {
        auto s = reinterpret_cast<const uint8_t*>(src);
        std::transform(&s[start], &s[start + length], dest,
            [](const uint8_t& v) -> std::complex<float> {
                const float k = 1.0f / 128.0f;
                return { (v - 127.4f) * k, 0 };
            }
        );
    }
};

InputSource::InputSource()
{
}

InputSource::~InputSource()
{
    cleanup();
}

void InputSource::cleanup()
{
    if (mmapData != nullptr) {
        inputFile->unmap(mmapData);
        mmapData = nullptr;
    }

    if (inputFile != nullptr) {
        delete inputFile;
        inputFile = nullptr;
    }
}

void InputSource::readMetaData(const char *filename)
{
    QString dataFilename;

    std::unique_ptr<QFile> dataFile(new QFile(dataFilename));
    QFile datafile(filename);
    if (!datafile.open(QFile::ReadOnly | QIODevice::Text)) {
        throw std::runtime_error((dataFilename + ": " + dataFile->errorString()).toStdString());
    }

    auto data = datafile.readAll();
    auto data_as_json = json::parse(data);
    sigmf::SigMF<sigmf::Global<core::DescrT>,
            sigmf::Capture<core::DescrT>,
            sigmf::Annotation<core::DescrT> > roundtripstuff = data_as_json;

    auto global_core = roundtripstuff.global.access<core::GlobalT>();
    //qDebug() << global_core.sample_rate;
    //qDebug() << QString::fromStdString(global_core.description);

    //sampleRate = global_core.sample_rate;
    setSampleRate(global_core.sample_rate);

    for(auto capture : roundtripstuff.captures) {
        auto core = capture.access<core::CaptureT>();
        frequency = core.frequency;
        //qDebug() << frequency;
    }

    for(auto annotation : roundtripstuff.annotations) {
        Annotation a;
        auto core = annotation.access<core::AnnotationT>();
        //std::cout << "Description: " << core.description << std::endl;
        //qDebug() << core.sample_start;
        //qDebug() << core.freq_lower_edge << " " << core.freq_upper_edge;


        //if(!annotation.contains("core:sample_start") ||
        //        !annotation.contains("core:sample_count")) {
        //    continue;
        //}

        a.sampleRange = range_t<size_t>{core.sample_start, core.sample_start + core.sample_count - 1};
        a.frequencyRange = range_t<double>{core.freq_lower_edge, core.freq_upper_edge};
        a.comment = QString::fromStdString(core.description);

        //qDebug() << "add " << a.comment << " " << a.sampleRange.minimum << " " << a.sampleRange.maximum << " " << a.frequencyRange.minimum << " " << a.frequencyRange.maximum;
        annotationList.append(a);
    }
}


void InputSource::openFile(const char *filename)
{
    QFileInfo fileInfo(filename);
    std::string suffix = std::string(fileInfo.suffix().toLower().toUtf8().constData());
    if(_fmt!=""){ suffix = _fmt; } // allow fmt override
    if ((suffix == "cfile") || (suffix == "cf32")  || (suffix == "fc32")) {
        sampleAdapter = std::unique_ptr<SampleAdapter>(new ComplexF32SampleAdapter());
    }
    else if ((suffix == "cs16") || (suffix == "sc16") || (suffix == "c16")) {
        sampleAdapter = std::unique_ptr<SampleAdapter>(new ComplexS16SampleAdapter());
    }
    else if ((suffix == "cs8") || (suffix == "sc8") || (suffix == "c8")) {
        sampleAdapter = std::unique_ptr<SampleAdapter>(new ComplexS8SampleAdapter());
    }
    else if ((suffix == "cu8") || (suffix == "uc8")) {
        sampleAdapter = std::unique_ptr<SampleAdapter>(new ComplexU8SampleAdapter());
    }
    else if (suffix == "f32") {
        sampleAdapter = std::unique_ptr<SampleAdapter>(new RealF32SampleAdapter());
        _realSignal = true;
    }
    else if (suffix == "s16") {
        sampleAdapter = std::unique_ptr<SampleAdapter>(new RealS16SampleAdapter());
        _realSignal = true;
    }
    else if (suffix == "s8") {
        sampleAdapter = std::unique_ptr<SampleAdapter>(new RealS8SampleAdapter());
        _realSignal = true;
    }
    else if (suffix == "u8") {
        sampleAdapter = std::unique_ptr<SampleAdapter>(new RealU8SampleAdapter());
        _realSignal = true;
    }
    else {
        sampleAdapter = std::unique_ptr<SampleAdapter>(new ComplexF32SampleAdapter());
    }

    QString dataFilename;
    QString metaFilename;
    //QFileInfo fileInfo(fileName);
    //std::string suffix = std::string(fileInfo.suffix().toLower().toUtf8().constData());

    if (suffix == "sigmf-meta") {
        //sampleAdapter = std::unique_ptr<SampleAdapter>(new ComplexF32SampleAdapter());
        dataFilename = fileInfo.path() + "/" + fileInfo.completeBaseName() + ".sigmf-data";
        metaFilename = filename;
        readMetaData(metaFilename.toUtf8().constData());
    }
    else if (suffix == "sigmf-data") {
        //sampleAdapter = std::unique_ptr<SampleAdapter>(new ComplexF32SampleAdapter());
        dataFilename = filename;
        metaFilename = fileInfo.path() + "/" + fileInfo.completeBaseName() + ".sigmf-meta";
        readMetaData(metaFilename.toUtf8().constData());
    }
    else if (suffix == "sigmf") {
        // TODO
        dataFilename = filename;
    }
    else {
        dataFilename = filename;
    }

    std::unique_ptr<QFile> file(new QFile(dataFilename));
    if (!file->open(QFile::ReadOnly)) {
        throw std::runtime_error(file->errorString().toStdString());
    }

    auto size = file->size();
    sampleCount = size / sampleAdapter->sampleSize();

    auto data = file->map(0, size);
    if (data == nullptr)
        throw std::runtime_error("Error mmapping file");

    cleanup();

    inputFile = file.release();
    mmapData = data;

    invalidate();
}

void InputSource::setSampleRate(double rate)
{
    sampleRate = rate;
    invalidate();
}

double InputSource::rate()
{
    return sampleRate;
}

std::unique_ptr<std::complex<float>[]> InputSource::getSamples(size_t start, size_t length)
{
    if (inputFile == nullptr)
        return nullptr;

    if (mmapData == nullptr)
        return nullptr;

    if(start < 0 || length < 0)
        return nullptr;

    if (start + length > sampleCount)
        return nullptr;

    std::unique_ptr<std::complex<float>[]> dest(new std::complex<float>[length]);
    sampleAdapter->copyRange(mmapData, start, length, dest.get());

    return dest;
}

void InputSource::setFormat(std::string fmt){
    _fmt = fmt;
}
