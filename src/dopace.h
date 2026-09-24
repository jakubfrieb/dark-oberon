/*
 * Game pace of this fork.
 *
 * The .rac files keep the original timings (worker repairing_time, factory product_time).
 * These factors speed the game up for every race. They are compiled in, so all players of a
 * network game (same version) use the same pace.
 */
#pragma once

/** Construction, upgrades and repairs are this many times faster than repairing_time says. */
const double PACE_BUILD_SPEED = 3.0;

/** Factories produce units this many times faster than product_time says. */
const double PACE_PRODUCTION_SPEED = 1.5;
