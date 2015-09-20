#pragma once

#include <QMainWindow>
#include <QScrollArea>
#include "spectrogram.h"
#include "spectrogramcontrols.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow();
    void openFile(QString fileName);
    void changeSampleRate(int rate);
    void changeCenterFreq(int rate);

public slots:
	void setSampleRate(QString rate);
	void setCenterFreq(QString rate);
	void setFFTSize(int size);
	void setZoomLevel(int zoom);

protected:
	bool eventFilter(QObject *obj, QEvent *event);

private:
    QScrollArea scrollArea;
    Spectrogram spectrogram;
    SpectrogramControls *dock;

	off_t getCenterSample();
	int getScrollPos(off_t sample);
};
