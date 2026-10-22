#include "remindme/flap_display.hpp"

#include <QEasingCurve>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QVariantAnimation>

namespace remindme
{

namespace
{
constexpr int kFlipDurationMs = 260;
constexpr int kTileGapPx = 4;
constexpr qreal kTileRadiusPx = 4.0;
}

FlapDisplay::FlapDisplay(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    m_flip = new QVariantAnimation(this);
    m_flip->setStartValue(0.0);
    m_flip->setEndValue(1.0);
    m_flip->setDuration(kFlipDurationMs);
    m_flip->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_flip, &QVariantAnimation::valueChanged, this, [this](const QVariant &value)
            {
                const qreal p = value.toReal();
                for (Cell &cell : m_cells)
                {
                    if (cell.progress < 1.0)
                        cell.progress = p;
                }
                update(); });
    connect(m_flip, &QVariantAnimation::finished, this, [this]()
            {
                for (Cell &cell : m_cells)
                    cell.progress = 1.0;
                update(); });
}

bool FlapDisplay::isSeparator(QChar c)
{
    return c == ':' || c == ' ';
}

// Deliberately ignores Windows "Show animations": the flip is the app's one short animation (Kyo's call).
// The env var is for screenshot captures, which need settled tiles.
bool FlapDisplay::reduceMotion()
{
    return qEnvironmentVariable("REMINDME_REDUCE_MOTION") == "1";
}

void FlapDisplay::setText(const QString &text)
{
    if (text == m_text)
        return;

    const bool sameShape = text.size() == m_text.size();
    m_text = text;

    if (!sameShape)
    {
        // A new layout (e.g. 59:59 -> 1:00:00) snaps instead of flipping every tile.
        m_cells.clear();
        for (QChar c : text)
            m_cells.push_back({c, c, 1.0});
        m_flip->stop();
        updateGeometry();
        update();
        return;
    }

    bool anyFlip = false;
    for (int i = 0; i < text.size(); ++i)
    {
        Cell &cell = m_cells[i];
        if (cell.current == text.at(i))
            continue;

        cell.previous = cell.current;
        cell.current = text.at(i);
        cell.progress = isSeparator(cell.current) ? 1.0 : 0.0;
        anyFlip = anyFlip || cell.progress < 1.0;
    }

    if (anyFlip && !reduceMotion())
    {
        m_flip->stop();
        m_flip->start();
    }
    else
    {
        for (Cell &cell : m_cells)
            cell.progress = 1.0;
    }
    update();
}

void FlapDisplay::setColors(const QColor &tile, const QColor &glyph, const QColor &seam)
{
    m_tile = tile;
    m_glyph = glyph;
    m_seam = seam;
    update();
}

void FlapDisplay::setGlyphPixelSize(int px)
{
    m_glyphPx = qMax(8, px);
    updateGeometry();
    update();
}

QFont FlapDisplay::glyphFont() const
{
    QFont font;
    font.setFamilies({"Segoe UI Variable Display", "Segoe UI"});
    font.setPixelSize(m_glyphPx);
    font.setWeight(QFont::DemiBold);
    return font;
}

int FlapDisplay::tileWidth() const
{
    // Every tile is as wide as the widest glyph showing, so letters (DUE NOW) fit like digits do.
    const QFontMetrics metrics(glyphFont());
    int widest = metrics.horizontalAdvance(QLatin1Char('8'));
    for (const Cell &cell : m_cells)
    {
        if (!isSeparator(cell.current))
            widest = qMax(widest, metrics.horizontalAdvance(cell.current));
    }
    return widest + m_glyphPx * 30 / 100;
}

int FlapDisplay::separatorWidth() const
{
    return m_glyphPx * 32 / 100;
}

int FlapDisplay::tileHeight() const
{
    return m_glyphPx * 132 / 100;
}

QSize FlapDisplay::sizeHint() const
{
    int width = 0;
    for (int i = 0; i < m_cells.size(); ++i)
    {
        width += isSeparator(m_cells[i].current) ? separatorWidth() : tileWidth();
        if (i + 1 < m_cells.size())
            width += kTileGapPx;
    }
    return {qMax(width, tileWidth()), tileHeight()};
}

void FlapDisplay::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setFont(glyphFont());

    const int h = tileHeight();
    const int top = (height() - h) / 2;
    int x = width() - sizeHint().width(); // right-aligned, like the label it replaces

    auto drawFace = [&](const QRectF &tile, QChar glyph)
    {
        QPainterPath path;
        path.addRoundedRect(tile, kTileRadiusPx, kTileRadiusPx);
        painter.fillPath(path, m_tile);
        // Real flaps are two-tone: the upper half catches a little more light.
        painter.save();
        // Intersect, not replace: while flipping, drawHalf() has already clipped to one half.
        painter.setClipRect(QRectF(tile.left(), tile.top(), tile.width(), tile.height() / 2.0), Qt::IntersectClip);
        painter.fillPath(path, m_tile.lighter(112));
        painter.restore();
        if (glyph == QLatin1Char('-'))
            return; // '-' is a blank tile (idle state); a dash would sit on the seam
        painter.setPen(m_glyph);
        painter.drawText(tile, Qt::AlignCenter, QString(glyph));
    };

    // Draws one half of a face, optionally folded toward the hinge by `scale` (1 = flat).
    auto drawHalf = [&](const QRectF &tile, QChar glyph, bool upper, qreal scale)
    {
        const qreal mid = tile.center().y();
        const QRectF half = upper ? QRectF(tile.left(), tile.top(), tile.width(), mid - tile.top())
                                  : QRectF(tile.left(), mid, tile.width(), tile.bottom() - mid);
        painter.save();
        painter.translate(0, mid);
        painter.scale(1.0, qMax<qreal>(0.001, scale));
        painter.translate(0, -mid);
        painter.setClipRect(half);
        drawFace(tile, glyph);
        if (scale < 1.0)
        {
            // Shade the moving flap so the fold reads as depth.
            QColor shade = m_seam;
            shade.setAlphaF(0.45 * (1.0 - scale));
            painter.fillRect(half, shade);
        }
        painter.restore();
    };

    for (const Cell &cell : m_cells)
    {
        if (isSeparator(cell.current))
        {
            const QRectF sep(x, top, separatorWidth(), h);
            if (cell.current == ':')
            {
                painter.setPen(m_glyph);
                painter.drawText(sep.translated(0, -m_glyphPx * 0.06), Qt::AlignCenter, QStringLiteral(":"));
            }
            x += separatorWidth() + kTileGapPx;
            continue;
        }

        const QRectF tile(x, top, tileWidth(), h);
        const qreal p = cell.progress;
        if (p >= 1.0 || cell.previous == cell.current)
        {
            drawFace(tile, cell.current);
        }
        else
        {
            drawHalf(tile, cell.current, true, 1.0);   // new top, revealed behind the falling flap
            drawHalf(tile, cell.previous, false, 1.0); // old bottom, covered as the flap lands
            if (p < 0.5)
                drawHalf(tile, cell.previous, true, 1.0 - (p * 2.0));
            else
                drawHalf(tile, cell.current, false, (p - 0.5) * 2.0);
        }

        // The seam every split-flap tile has.
        painter.fillRect(QRectF(tile.left(), tile.center().y() - 0.5, tile.width(), 1.0), m_seam);
        x += tileWidth() + kTileGapPx;
    }
}

}
