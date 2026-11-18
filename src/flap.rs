// SPDX-License-Identifier: MIT
//! Split-flap countdown tiles for the board: each changed tile flips once (260 ms).

use iced::widget::canvas::{self, Frame, Geometry, Path, Text};
use iced::{
    Color, Element, Length, Point, Rectangle, Renderer, Size, Theme, Vector, alignment, border,
    mouse,
};
use std::time::{Duration, Instant};

const FLIP: Duration = Duration::from_millis(260);
const GAP: f32 = 4.0;
const RADIUS: f32 = 4.0;

#[derive(Clone, Copy)]
struct Cell {
    previous: char,
    current: char,
}

#[derive(Default)]
pub struct Flap {
    text: String,
    cells: Vec<Cell>,
    started: Option<Instant>,
}

fn is_separator(c: char) -> bool {
    c == ':' || c == ' '
}

/// Screenshot captures set this to get settled tiles.
fn reduce_motion() -> bool {
    std::env::var("REMINDME_REDUCE_MOTION").is_ok_and(|v| v == "1")
}

/// Rough advance width of a glyph as a share of the pixel size (no font metrics in a canvas).
fn advance(c: char) -> f32 {
    match c {
        'M' | 'W' => 0.92,
        'A'..='Z' => 0.7,
        _ => 0.6,
    }
}

impl Flap {
    pub fn set_text(&mut self, text: &str, now: Instant) {
        if text == self.text {
            return;
        }
        let same_shape = text.chars().count() == self.cells.len();
        self.text = text.to_string();
        if !same_shape {
            // A new layout (59:59 -> 1:00:00) snaps instead of flipping every tile.
            self.cells = text
                .chars()
                .map(|c| Cell {
                    previous: c,
                    current: c,
                })
                .collect();
            self.started = None;
            return;
        }
        let mut any = false;
        for (cell, c) in self.cells.iter_mut().zip(text.chars()) {
            if cell.current != c {
                cell.previous = cell.current;
                cell.current = c;
                any |= !is_separator(c);
            } else {
                cell.previous = c;
            }
        }
        self.started = (any && !reduce_motion()).then_some(now);
    }

    pub fn animating(&self, now: Instant) -> bool {
        self.started.is_some_and(|s| now.duration_since(s) < FLIP)
    }

    fn progress(&self, now: Instant) -> f32 {
        let Some(start) = self.started else {
            return 1.0;
        };
        // Linear here; each half of the flip eases on its own (see `flip_pieces`).
        (now.duration_since(start).as_secs_f32() / FLIP.as_secs_f32()).min(1.0)
    }

    fn tile_width(&self, px: f32) -> f32 {
        let widest = self
            .cells
            .iter()
            .filter(|c| !is_separator(c.current))
            .map(|c| advance(c.current))
            .fold(0.6, f32::max);
        (widest + 0.3) * px
    }

    pub fn view<'a, M: 'a>(
        &'a self,
        px: f32,
        tile: Color,
        glyph: Color,
        seam: Color,
    ) -> Element<'a, M> {
        let tw = self.tile_width(px);
        let width: f32 = self
            .cells
            .iter()
            .map(|c| {
                if is_separator(c.current) {
                    px * 0.32
                } else {
                    tw
                }
            })
            .sum::<f32>()
            + GAP * self.cells.len().saturating_sub(1) as f32;
        let height = px * 1.32;
        iced::widget::canvas(FlapView {
            flap: self,
            px,
            tile,
            glyph,
            seam,
            progress: self.progress(Instant::now()),
        })
        .width(Length::Fixed(width.max(tw)))
        .height(Length::Fixed(height))
        .into()
    }
}

struct FlapView<'a> {
    flap: &'a Flap,
    px: f32,
    tile: Color,
    glyph: Color,
    seam: Color,
    progress: f32,
}

fn lighter(c: Color, by: f32) -> Color {
    Color {
        r: (c.r * by).min(1.0),
        g: (c.g * by).min(1.0),
        b: (c.b * by).min(1.0),
        a: c.a,
    }
}

/// Half a tile body: the top half keeps the top corners rounded, the bottom half the bottom ones.
fn half_body(rect: Rectangle, top: bool) -> Path {
    let h = rect.height / 2.0;
    let (y, radius) = if top {
        (
            rect.y,
            border::Radius {
                top_left: RADIUS,
                top_right: RADIUS,
                bottom_right: 0.0,
                bottom_left: 0.0,
            },
        )
    } else {
        (
            rect.y + h,
            border::Radius {
                top_left: 0.0,
                top_right: 0.0,
                bottom_right: RADIUS,
                bottom_left: RADIUS,
            },
        )
    };
    Path::rounded_rectangle(Point::new(rect.x, y), Size::new(rect.width, h), radius)
}

/// Splits a flip in progress into disjoint pieces: (glyph, region, squash) per piece.
/// `squash` scales the face toward the hinge: the falling flap folds the old top half down
/// (ease-in, it speeds up as it falls), then the new bottom half unfolds and lands (ease-out).
fn flip_pieces(
    rect: Rectangle,
    previous: char,
    current: char,
    p: f32,
) -> [(char, Rectangle, f32); 3] {
    let half = rect.height / 2.0;
    let mid = rect.y + half;
    let band = |y0: f32, y1: f32| Rectangle {
        y: y0,
        height: (y1 - y0).max(0.0),
        ..rect
    };
    let bottom = rect.y + rect.height;
    if p < 0.5 {
        let u = p * 2.0;
        let fh = half * (1.0 - u * u);
        [
            (current, band(rect.y, mid - fh), 1.0),
            (previous, band(mid - fh, mid), fh / half),
            (previous, band(mid, bottom), 1.0),
        ]
    } else {
        let u = (p - 0.5) * 2.0;
        let gh = half * (1.0 - (1.0 - u).powi(3));
        [
            (current, band(rect.y, mid), 1.0),
            (current, band(mid, mid + gh), gh / half),
            (previous, band(mid + gh, bottom), 1.0),
        ]
    }
}

impl FlapView<'_> {
    fn glyph_text(&self, c: char, rect: Rectangle, color: Color) -> Text {
        Text {
            content: c.to_string(),
            position: rect.center(),
            color,
            size: self.px.into(),
            font: crate::theme::SEMIBOLD,
            align_x: alignment::Horizontal::Center.into(),
            align_y: alignment::Vertical::Center,
            ..Text::default()
        }
    }

    /// Draws face `c` of the tile at `rect` into its own geometry, clipped to `region`.
    ///
    /// Every piece of a tile is a separate geometry with bounds that never overlap: iced draws
    /// canvas text above all fills of a layer and clips only per geometry (tiny-skia ignores
    /// `Frame::with_clip`), so overlapping pieces would show two glyphs on top of each other.
    /// `squash` < 1 folds the face toward the hinge and shades it.
    fn piece(
        &self,
        renderer: &Renderer,
        out: &mut Vec<Geometry>,
        rect: Rectangle,
        c: char,
        region: Rectangle,
        squash: f32,
    ) {
        if region.height < 0.5 {
            return;
        }
        let mut f = Frame::with_bounds(renderer, region);
        let shade = 1.0 - 0.4 * (1.0 - squash).clamp(0.0, 1.0);
        if squash < 1.0 {
            let hinge = rect.y + rect.height / 2.0;
            f.translate(Vector::new(0.0, hinge));
            f.scale_nonuniform(Vector::new(1.0, squash.max(0.01)));
            f.translate(Vector::new(0.0, -hinge));
        }
        f.fill(&half_body(rect, true), lighter(self.tile, 1.12 * shade));
        f.fill(&half_body(rect, false), lighter(self.tile, shade));
        if c != '-' {
            f.fill_text(self.glyph_text(c, rect, lighter(self.glyph, shade)));
        }
        out.push(f.into_geometry());
    }
}

impl<M> canvas::Program<M> for FlapView<'_> {
    type State = ();

    fn draw(
        &self,
        _: &(),
        renderer: &Renderer,
        _: &Theme,
        bounds: Rectangle,
        _: mouse::Cursor,
    ) -> Vec<Geometry> {
        let mut out = Vec::new();
        // Separators and seams: drawn last so the seams sit on top of the tile bodies.
        let mut top = Frame::new(renderer, bounds.size());
        let h = self.px * 1.32;
        let tw = self.flap.tile_width(self.px);
        let mut x = 0.0;
        for cell in &self.flap.cells {
            if is_separator(cell.current) {
                let w = self.px * 0.32;
                if cell.current == ':' {
                    let rect = Rectangle::new(Point::new(x, -self.px * 0.06), Size::new(w, h));
                    top.fill_text(self.glyph_text(':', rect, self.glyph));
                }
                x += w + GAP;
                continue;
            }
            let rect = Rectangle::new(Point::new(x, 0.0), Size::new(tw, h));
            let p = self.progress;
            if p >= 1.0 || cell.previous == cell.current {
                self.piece(renderer, &mut out, rect, cell.current, rect, 1.0);
            } else {
                for (c, region, squash) in flip_pieces(rect, cell.previous, cell.current, p) {
                    self.piece(renderer, &mut out, rect, c, region, squash);
                }
            }
            // The seam every split-flap tile has.
            top.fill_rectangle(Point::new(x, h / 2.0 - 0.5), Size::new(tw, 1.0), self.seam);
            x += tw + GAP;
        }
        out.push(top.into_geometry());
        out
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn tile() -> Rectangle {
        Rectangle::new(Point::new(10.0, 0.0), Size::new(40.0, 60.0))
    }

    #[test]
    fn flip_pieces_tile_the_whole_face_without_overlap() {
        let rect = tile();
        for step in 0..=20 {
            let p = step as f32 / 20.0 * 0.999;
            let pieces = flip_pieces(rect, '5', '4', p);
            let mut y = rect.y;
            for (_, region, squash) in pieces {
                assert!((region.y - y).abs() < 1e-3, "gap or overlap at p={p}");
                assert!(region.height >= 0.0);
                assert!((0.0..=1.0).contains(&squash));
                y = region.y + region.height;
            }
            assert!(
                (y - (rect.y + rect.height)).abs() < 1e-3,
                "face not covered at p={p}"
            );
        }
    }

    #[test]
    fn flip_starts_on_old_digit_and_ends_on_new() {
        let rect = tile();
        let start = flip_pieces(rect, '5', '4', 0.0);
        assert!(start[0].1.height < 1e-3, "new top hidden at start");
        assert_eq!((start[1].0, start[2].0), ('5', '5'));
        let end = flip_pieces(rect, '5', '4', 0.999);
        assert!(end[2].1.height < 0.5, "old bottom covered at the end");
        assert_eq!((end[0].0, end[1].0), ('4', '4'));
    }

    #[test]
    fn progress_is_linear_in_time() {
        let now = Instant::now();
        let flap = Flap {
            started: Some(now),
            ..Flap::default()
        };
        let p = flap.progress(now + FLIP / 2);
        assert!((p - 0.5).abs() < 0.01, "p = {p}");
    }
}
