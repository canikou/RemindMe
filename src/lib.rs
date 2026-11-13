// SPDX-License-Identifier: MIT
//! RemindMe core: parsing, scheduling, storage and update rules. No UI code lives here.

pub mod edit;
pub mod greetings;
pub mod model;
pub mod parser;
pub mod schedule;
pub mod store;
pub mod time_format;
pub mod update;
pub mod weekday;

pub const APP_NAME: &str = "RemindMe";
pub const APP_VERSION: &str = env!("CARGO_PKG_VERSION");
