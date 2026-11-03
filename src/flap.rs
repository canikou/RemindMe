// SPDX-License-Identifier: MIT
//! Split-flap countdown tiles for the board: each changed tile flips once (260 ms).

use iced::widget::canvas::{self, Frame, Geometry, Path, Text};
use iced::{Color, Element, Length, Point, Rectangle, Renderer, Size, Theme, alignment, mouse};
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
        let t = (now.duration_since(start).as_secs_f32() / FLIP.as_secs_f32()).min(1.0);
        1.0 - (1.0 - t).powi(3) // OutCubic
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

impl FlapView<'_> {
    fn glyph_text(&self, c: char, rect: Rectangle) -> Text {
        Text {
            content: c.to_string(),
            position: rect.center(),
            color: self.glyph,
            size: self.px.into(),
            font: crate::theme::SEMIBOLD,
            align_x: alignment::Horizontal::Center.into(),
            align_y: alignment::Vertical::Center,
            ..Text::default()
        }
    }

    /// One tile face clipped to `clip`: two-tone body plus its glyph ('-' is a blank tile).
    fn face(&self, frame: &mut Frame, rect: Rectangle, c: char, clip: Rectangle) {
        frame.with_clip(clip, |f| {
            let body = Path::rounded_rectangle(rect.position(), rect.size(), RADIUS.into());
            f.fill(&body, self.tile);
            let top = Rectangle {
                height: rect.height / 2.0,
                ..rect
            };
            if let Some(top_clip) = top.intersection(&clip) {
                f.with_clip(top_clip, |t| t.fill(&body, lighter(self.tile, 1.12)));
            }
            if c != '-' {
                f.fill_text(self.glyph_text(c, rect));
            }
        });
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
        let mut frame = Frame::new(renderer, bounds.size());
        let h = self.px * 1.32;
        let tw = self.flap.tile_width(self.px);
        let mut x = 0.0;
        for cell in &self.flap.cells {
            if is_separator(cell.current) {
                let w = self.px * 0.32;
                if cell.current == ':' {
                    let rect = Rectangle::new(Point::new(x, -self.px * 0.06), Size::new(w, h));
                    frame.fill_text(self.glyph_text(':', rect));
                }
                x += w + GAP;
                continue;
            }
            let rect = Rectangle::new(Point::new(x, 0.0), Size::new(tw, h));
            let mid = h / 2.0;
            let upper = Rectangle {
                height: mid,
                ..rect
            };
            let lower = Rectangle {
                y: mid,
                height: h - mid,
                ..rect
            };
            let p = self.progress;
            if p >= 1.0 || cell.previous == cell.current {
                self.face(&mut frame, rect, cell.current, rect);
            } else {
                self.face(&mut frame, rect, cell.current, upper); // new top, revealed behind the falling flap
                self.face(&mut frame, rect, cell.previous, lower); // old bottom, covered as it lands
                // The moving flap shrinks toward the hinge, then grows down from it.
                let (c, flap) = if p < 0.5 {
                    let fh = mid * (1.0 - p * 2.0);
                    (
                        cell.previous,
                        Rectangle {
                            y: mid - fh,
                            height: fh,
                            ..rect
                        },
                    )
                } else {
                    (
                        cell.current,
                        Rectangle {
                            y: mid,
                            height: mid * (p - 0.5) * 2.0,
                            ..rect
                        },
                    )
                };
                if flap.height > 0.5 {
                    self.face(&mut frame, rect, c, flap);
                    let fold = (1.0 - flap.height / mid).clamp(0.0, 1.0);
                    frame.fill_rectangle(
                        flap.position(),
                        flap.size(),
                        Color {
                            a: 0.45 * fold,
                            ..self.seam
                        },
                    );
                }
            }
            // The seam every split-flap tile has.
            frame.fill_rectangle(Point::new(x, mid - 0.5), Size::new(tw, 1.0), self.seam);
            x += tw + GAP;
        }
        vec![frame.into_geometry()]
    }
}
