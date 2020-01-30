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

#pragma once

#include "util.h"
#include <QString>
#include <QObject>

#if 1
class Annotation
{
public:
    range_t<size_t> sampleRange;
    range_t<double> frequencyRange;
    //QString *comment;
    QString comment;
};
#endif

class MetaDataSource : public QObject
{
    Q_OBJECT
private:
    double frequency;
public:
    MetaDataSource();
    ~MetaDataSource();
    void openFile(const char *filename);
    QList<Annotation> annotationList;
    double getFrequency();
};
