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

#include "metadatasource.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <stdexcept>
#include <algorithm>
#include <iostream>
#include <memory>

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

MetaDataSource::MetaDataSource()
{
}

MetaDataSource::~MetaDataSource()
{
}

void MetaDataSource::openFile(const char *filename)
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

    sampleRate = global_core.sample_rate;

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

double MetaDataSource::getFrequency()
{
    return frequency;
}

double MetaDataSource::getSampleRate()
{
    return sampleRate;
}
