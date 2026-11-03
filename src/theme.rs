// SPDX-License-Identifier: MIT
//! The departures-board palette from DESIGN.md and the widget styles built on it.

use iced::widget::{button, checkbox, container, scrollable, text_editor, text_input};
use iced::{Background, Border, Color, Font, Shadow, Theme, color, font};

#[derive(Clone, Copy, Debug)]
pub struct Palette {
    pub dark: bool,
    pub ground: Color,
    pub surface: Color,
    pub surface_alt: Color,
    pub border: Color,
    pub rule: Color,
    pub text: Color,
    pub muted: Color,
    pub board: Color,
    pub board_raised: Color,
    pub board_text: Color,
    pub board_muted: Color,
    pub accent: Color,
    pub due_text: Color,
    pub danger: Color,
    pub focus: Color,
    pub primary: Color,
    pub primary_hover: Color,
    pub primary_text: Color,
}

pub const LIGHT: Palette = Palette {
    dark: false,
    ground: color!(0xF3F4F7),
    surface: color!(0xFFFFFF),
    surface_alt: color!(0xF7F8FA),
    border: color!(0xDDE1E8),
    rule: color!(0xECEEF2),
    text: color!(0x16213A),
    muted: color!(0x566075),
    board: color!(0x16213A),
    board_raised: color!(0x22305A),
    board_text: color!(0xFFFFFF),
    board_muted: color!(0xB4BED2),
    accent: color!(0xF5C84B),
    due_text: color!(0x8A6100),
    danger: color!(0xB3261E),
    focus: color!(0x3B6FD8),
    primary: color!(0x16213A),
    primary_hover: color!(0x22305A),
    primary_text: color!(0xFFFFFF),
};

pub const DARK: Palette = Palette {
    dark: true,
    ground: color!(0x10141C),
    surface: color!(0x171C26),
    surface_alt: color!(0x1C2230),
    border: color!(0x2C3446),
    rule: color!(0x232A39),
    text: color!(0xE8ECF4),
    muted: color!(0xA3ACBE),
    board: color!(0x1B2742),
    board_raised: color!(0x26355A),
    board_text: color!(0xFFFFFF),
    board_muted: color!(0xB4BED2),
    accent: color!(0xF5C84B),
    due_text: color!(0xF5C84B),
    danger: color!(0xFF8A80),
    focus: color!(0x7EA6FF),
    primary: color!(0x2B3D66),
    primary_hover: color!(0x364B7D),
    primary_text: color!(0xFFFFFF),
};

/// `REMINDME_THEME=dark|light` overrides the system setting.
pub fn forced_dark() -> Option<bool> {
    match std::env::var("REMINDME_THEME")
        .unwrap_or_default()
        .trim()
        .to_lowercase()
        .as_str()
    {
        "dark" => Some(true),
        "light" => Some(false),
        _ => None,
    }
}

pub fn iced_theme(p: &Palette) -> Theme {
    Theme::custom(
        if p.dark {
            "RemindMe Dark"
        } else {
            "RemindMe Light"
        },
        iced::theme::Palette {
            background: p.ground,
            text: p.text,
            primary: p.focus,
            success: p.focus,
            warning: p.accent,
            danger: p.danger,
        },
    )
}

pub const SEMIBOLD: Font = Font {
    weight: font::Weight::Semibold,
    ..Font::DEFAULT
};
pub const BOLD: Font = Font {
    weight: font::Weight::Bold,
    ..Font::DEFAULT
};

/// Qt point sizes from 1.x, at 96 dpi.
pub const fn pt(points: f32) -> f32 {
    points * 4.0 / 3.0
}

fn border(c: Color, radius: f32) -> Border {
    Border {
        color: c,
        width: 1.0,
        radius: radius.into(),
    }
}

pub fn panel(bg: Color, edge: Option<Color>, radius: f32) -> impl Fn(&Theme) -> container::Style {
    move |_| container::Style {
        background: Some(Background::Color(bg)),
        border: edge.map_or_else(Border::default, |c| border(c, radius)),
        ..Default::default()
    }
}

#[derive(Clone, Copy, PartialEq)]
pub enum ButtonKind {
    Normal,
    Primary,
    RowAction,
    RowDanger,
    Board,
    BoardChecked,
}

pub fn button_style(
    p: Palette,
    kind: ButtonKind,
) -> impl Fn(&Theme, button::Status) -> button::Style {
    move |_, status| {
        let hovered = matches!(status, button::Status::Hovered);
        let pressed = matches!(status, button::Status::Pressed);
        let disabled = matches!(status, button::Status::Disabled);
        let (bg, fg, edge) = match kind {
            ButtonKind::Normal => (
                if pressed {
                    p.rule
                } else if hovered {
                    p.surface_alt
                } else if disabled {
                    p.ground
                } else {
                    p.surface
                },
                if disabled { p.muted } else { p.text },
                p.border,
            ),
            ButtonKind::Primary => {
                let bg = if hovered || pressed {
                    p.primary_hover
                } else {
                    p.primary
                };
                (bg, p.primary_text, bg)
            }
            ButtonKind::RowAction | ButtonKind::RowDanger => {
                let hot = if kind == ButtonKind::RowDanger {
                    p.danger
                } else {
                    p.text
                };
                if hovered || pressed {
                    (p.surface_alt, hot, p.border)
                } else {
                    (Color::TRANSPARENT, p.muted, Color::TRANSPARENT)
                }
            }
            ButtonKind::Board => (
                if hovered || pressed {
                    p.board_raised
                } else {
                    Color::TRANSPARENT
                },
                if disabled {
                    p.board_muted
                } else {
                    p.board_text
                },
                Color::TRANSPARENT,
            ),
            ButtonKind::BoardChecked => (p.board_raised, p.board_text, p.board_muted),
        };
        button::Style {
            background: Some(Background::Color(bg)),
            text_color: fg,
            border: border(edge, 6.0),
            shadow: Shadow::default(),
            snap: true,
        }
    }
}

pub fn input_style(p: Palette) -> impl Fn(&Theme, text_input::Status) -> text_input::Style {
    move |_, status| text_input::Style {
        background: Background::Color(p.surface),
        border: border(
            if matches!(status, text_input::Status::Focused { .. }) {
                p.focus
            } else {
                p.border
            },
            6.0,
        ),
        icon: p.muted,
        placeholder: p.muted,
        value: p.text,
        selection: Color { a: 0.35, ..p.focus },
    }
}

pub fn editor_style(p: Palette) -> impl Fn(&Theme, text_editor::Status) -> text_editor::Style {
    move |_, status| text_editor::Style {
        background: Background::Color(p.surface),
        border: border(
            if matches!(status, text_editor::Status::Focused { .. }) {
                p.focus
            } else {
                p.border
            },
            6.0,
        ),
        placeholder: p.muted,
        value: p.text,
        selection: Color { a: 0.35, ..p.focus },
    }
}

pub fn checkbox_style(
    p: Palette,
    muted_label: bool,
) -> impl Fn(&Theme, checkbox::Status) -> checkbox::Style {
    move |_, status| {
        let (checked, hovered) = match status {
            checkbox::Status::Active { is_checked } => (is_checked, false),
            checkbox::Status::Hovered { is_checked } => (is_checked, true),
            checkbox::Status::Disabled { is_checked } => (is_checked, false),
        };
        checkbox::Style {
            background: Background::Color(if checked { p.text } else { p.surface }),
            icon_color: p.surface,
            border: border(
                if hovered {
                    p.focus
                } else if checked {
                    p.text
                } else {
                    p.muted
                },
                3.0,
            ),
            text_color: Some(if muted_label { p.muted } else { p.text }),
        }
    }
}

pub fn scroll_style(p: Palette) -> impl Fn(&Theme, scrollable::Status) -> scrollable::Style {
    move |theme, status| {
        let mut s = scrollable::default(theme, status);
        let rail = scrollable::Rail {
            background: None,
            border: Border::default(),
            scroller: scrollable::Scroller {
                background: Background::Color(p.border),
                border: Border {
                    radius: 4.0.into(),
                    ..Border::default()
                },
            },
        };
        s.vertical_rail = rail;
        s.horizontal_rail = rail;
        s.container = container::Style::default();
        s
    }
}
