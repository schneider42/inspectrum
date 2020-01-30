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
    QFile data(filename);
    if (!data.open(QFile::ReadOnly | QIODevice::Text)) {
        throw std::runtime_error((dataFilename + ": " + dataFile->errorString()).toStdString());
    }

    QJsonDocument d = QJsonDocument::fromJson(data.readAll());
    qDebug() << d.isNull();
    QJsonObject obj = d.object();

    qDebug() << obj["global"].toObject()["core:sample_rate"].toInt();
    qDebug() << obj["global"].toObject()["core:description"].toString();

    //uint64_t frequency = 0;
    if(obj.contains("captures")) {
        QJsonArray captures = obj["captures"].toArray();
        for(auto&& item: captures) {
            const QJsonObject& capture = item.toObject();
            if(capture.contains("core:frequency")) {
                //frequency = capture["core:frequency"].toVariant().toLongLong();
                frequency = capture["core:frequency"].toVariant().toDouble();
                qDebug() << frequency;
            }
        }
    }

    QJsonArray annotations = obj["annotations"].toArray();

    for(auto&& item: annotations)
    {
        Annotation a;
        const QJsonObject& annotation = item.toObject();
        if(!annotation.contains("core:sample_start") ||
                !annotation.contains("core:sample_count")) {
            continue;
        }

        // Ugly hack to get out something bigger than a 32 bit int via toInt()
        //qDebug() << annotation["core:sample_start"].toVariant().toLongLong();
        uint64_t sample_start =  annotation["core:sample_start"].toVariant().toLongLong();// * 40;
        uint64_t sample_count =  annotation["core:sample_count"].toVariant().toLongLong();// * 40;
        a.sampleRange = range_t<size_t>{sample_start, sample_start + sample_count - 1};

        qDebug() << sample_start;

        double freq_lower_edge = 0;
        double freq_upper_edge = 0;

        if(annotation.contains("core:freq_lower_edge") &&
                annotation.contains("core:freq_upper_edge")) {
            freq_lower_edge =  annotation["core:freq_lower_edge"].toDouble();
            freq_upper_edge =  annotation["core:freq_upper_edge"].toDouble();

            qDebug() << freq_lower_edge << " " << freq_upper_edge;
        }

        a.frequencyRange = range_t<double>{freq_lower_edge, freq_upper_edge};

        if(annotation.contains("core:comment")) {
            a.comment = annotation["core:comment"].toString();
            qDebug() << a.comment;
        }
        //qDebug() << annotation["core:sample_startt"].toVariant().toLongLong();
        annotationList.append(a);
    }
}

double MetaDataSource::getFrequency()
{
    return frequency;
}
