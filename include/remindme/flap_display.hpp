#pragma once
#include <QColor>
#include <QString>
#include <QVector>
#include <QWidget>

class QVariantAnimation;

namespace remindme
{

// Split-flap readout: each character sits on its own tile and flips when it changes.
class FlapDisplay : public QWidget
{
    Q_OBJECT

public:
    explicit FlapDisplay(QWidget *parent = nullptr);

    void setText(const QString &text);
    QString text() const { return m_text; }

    void setColors(const QColor &tile, const QColor &glyph, const QColor &seam);
    void setGlyphPixelSize(int px);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    struct Cell
    {
        QChar current;
        QChar previous;
        qreal progress = 1.0; // 1 = settled
    };

    static bool isSeparator(QChar c);
    static bool reduceMotion();
    QFont glyphFont() const;
    int tileWidth() const;
    int separatorWidth() const;
    int tileHeight() const;

    QString m_text;
    QVector<Cell> m_cells;
    QColor m_tile;
    QColor m_glyph;
    QColor m_seam;
    int m_glyphPx = 52;
    QVariantAnimation *m_flip = nullptr;
};

}
