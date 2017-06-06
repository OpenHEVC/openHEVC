#ifndef OHVZGLFRAMEVIEW_H
#define OHVZGLFRAMEVIEW_H

#include <QOpenGLWidget>

class QLabel;
class ohvzModel;

class ohvzGLFrameView : public QOpenGLWidget
{
public:
    explicit ohvzGLFrameView(ohvzModel *model, QWidget* parent = NULL);
    ~ohvzGLFrameView();

    QImage createImageWithOverlay(const QImage& baseImage, const QImage& overlayImage);
    void setImage(QImage *image);
    void display();
    void displayFrame();
    void displayTiles();
    void displayCTB();

protected:
    void paintGL();

private:
    QImage *m_frameImage;
    QLabel *m_frameLabel;
    ohvzModel *m_model;

signals:

public slots:
};

#endif // OHVZGLFRAMEVIEW_H
