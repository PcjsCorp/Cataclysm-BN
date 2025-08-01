#include "game.h" // IWYU pragma: associated

#include <algorithm>
#include <sstream>
#include <type_traits>
#include <utility>

#include "avatar.h"
#include "calendar.h"
#include "color.h"
#include "harvest.h"
#include "input.h"
#include "map.h"
#include "mapdata.h"
#include "mod_manager.h"
#include "output.h"
#include "string_formatter.h"
#include "string_id.h"
#include "string_utils.h"
#include "translations.h"
#include "ui_manager.h"
#include "item_group.h"
#include "itype.h"

static const skill_id skill_survival( "survival" );

static const trait_id trait_ILLITERATE( "ILLITERATE" );
static const trait_id trait_DEBUG_NIGHTVISION( "DEBUG_NIGHTVISION" );

enum class description_target : int {
    creature,
    furniture,
    terrain
};

static bool debug_vision()
{
    return debug_mode || get_player_character().has_trait( trait_DEBUG_NIGHTVISION );
}

static const Creature *seen_critter( const game &g, const tripoint &p )
{
    const Creature *critter = g.critter_at( p, true );
    if( critter != nullptr && g.u.sees( *critter ) ) {
        return critter;
    }

    return nullptr;
}

void game::extended_description( const tripoint &p )
{
    ui_adaptor ui;
    const int top = 3;
    int width = 0;
    catacurses::window w_head;
    catacurses::window w_main;
    ui.on_screen_resize( [&]( ui_adaptor & ui ) {
        const int left = 0;
        const int right = TERMX;
        const int bottom = TERMY;
        width = right - left;
        const int height = bottom - top;
        w_head = catacurses::newwin( top, TERMX, point_zero );
        w_main = catacurses::newwin( height, width, point( left, top ) );
        ui.position( point_zero, point( TERMX, TERMY ) );
    } );
    ui.mark_resize();

    // Default to critter (if any), furniture (if any), then terrain.
    description_target cur_target = description_target::terrain;
    if( seen_critter( *this, p ) != nullptr ) {
        cur_target = description_target::creature;
    } else if( get_map().has_furn( p ) ) {
        cur_target = description_target::furniture;
    }

    std::string action;
    input_context ctxt( "EXTENDED_DESCRIPTION" );
    ctxt.register_action( "CREATURE" );
    ctxt.register_action( "FURNITURE" );
    ctxt.register_action( "TERRAIN" );
    ctxt.register_action( "CONFIRM" );
    ctxt.register_action( "QUIT" );
    ctxt.register_action( "HELP_KEYBINDINGS" );

    ui.on_redraw( [&]( const ui_adaptor & ) {
        werase( w_head );
        mvwprintz( w_head, point_zero, c_white,
                   _( "[%s] describe creatures, [%s] describe furniture, "
                      "[%s] describe terrain, [%s] close." ),
                   ctxt.get_desc( "CREATURE" ), ctxt.get_desc( "FURNITURE" ),
                   ctxt.get_desc( "TERRAIN" ), ctxt.get_desc( "QUIT" ) );

        // Set up line drawings
        for( int i = 0; i < TERMX; i++ ) {
            mvwputch( w_head, point( i, top - 1 ), c_white, LINE_OXOX );
        }

        wnoutrefresh( w_head );

        std::string desc;
        // Allow looking at invisible tiles - player may want to examine hallucinations etc.
        switch( cur_target ) {
            case description_target::creature: {
                const Creature *critter = seen_critter( *this, p );
                if( critter != nullptr ) {
                    desc = critter->extended_description();
                } else {
                    desc = _( "You do not see any creature here." );
                }
            }
            break;
            case description_target::furniture:
                if( !u.sees( p ) || !m.has_furn( p ) ) {
                    desc = _( "You do not see any furniture here." );
                } else {
                    const furn_id fid = m.furn( p );
                    if( display_mod_source ) {
                        const std::string mod_src = enumerate_as_string( fid->src.begin(),
                        fid->src.end(), []( const std::pair<furn_str_id, mod_id> &source ) {
                            return string_format( "'%s'", source.second->name() );
                        }, enumeration_conjunction::arrow );
                        desc = string_format( _( "Origin: %s\n" ), mod_src );
                    }
                    if( display_object_ids ) {
                        desc += colorize( string_format( "[%s]\n", fid.id() ), c_light_blue );
                    }
                    desc += fid.obj().extended_description();
                }
                break;
            case description_target::terrain:
                if( !u.sees( p ) ) {
                    desc = _( "You can't see the terrain here." );
                } else {
                    const ter_id tid = m.ter( p );
                    if( display_mod_source ) {
                        const std::string mod_src = enumerate_as_string( tid->src.begin(),
                        tid->src.end(), []( const std::pair<ter_str_id, mod_id> &source ) {
                            return string_format( "'%s'", source.second->name() );
                        }, enumeration_conjunction::arrow );
                        desc = string_format( _( "Origin: %s\n" ), mod_src );
                    }
                    if( display_object_ids ) {
                        desc += colorize( string_format( "[%s]\n", tid.id() ), c_light_blue );
                    }
                    desc += tid.obj().extended_description();
                }
                break;
        }

        std::string signage = m.get_signage( p );
        if( !signage.empty() ) {
            // NOLINTNEXTLINE(cata-text-style): the question mark does not end a sentence
            desc += u.has_trait( trait_ILLITERATE ) ? _( "\nSign: ???" ) : string_format( _( "\nSign: %s" ),
                    signage );
        }

        werase( w_main );
        fold_and_print_from( w_main, point_zero, width, 0, c_light_gray, desc );
        wnoutrefresh( w_main );
    } );

    do {
        ui_manager::redraw();
        action = ctxt.handle_input();
        if( action == "CREATURE" ) {
            cur_target = description_target::creature;
        } else if( action == "FURNITURE" ) {
            cur_target = description_target::furniture;
        } else if( action == "TERRAIN" ) {
            cur_target = description_target::terrain;
        }
    } while( action != "CONFIRM" && action != "QUIT" );
}

std::string map_data_common_t::extended_description() const
{
    std::stringstream ss;
    ss << "<header>" << string_format( _( "That is a %s." ), name() ) << "</header>" << '\n';
    ss << description << '\n';
    bool has_any_harvest = std::ranges::any_of( harvest_by_season,
    []( const harvest_id & hv ) {
        return !hv.obj().empty();
    } );

    if( has_any_harvest ) {
        ss << "--" << '\n';
        int player_skill = get_avatar().get_skill_level( skill_survival );
        ss << _( "You could harvest the following things from it:" ) << '\n';
        // Group them by identical ids to avoid repeating same blocks of data
        // First, invert the mapping: season->id to id->seasons
        std::multimap<harvest_id, season_type> identical_harvest;
        for( size_t season = SPRING; season <= WINTER; season++ ) {
            const auto &hv = harvest_by_season[ season ];
            if( hv.obj().empty() ) {
                continue;
            }

            identical_harvest.insert( std::make_pair( hv, static_cast<season_type>( season ) ) );
        }
        // Now print them in order of seasons
        // TODO: Highlight current season
        for( size_t season = SPRING; season <= WINTER; season++ ) {
            const auto range = identical_harvest.equal_range( harvest_by_season[ season ] );
            if( range.first == range.second ) {
                continue;
            }

            // List the seasons first
            ss << enumerate_as_string( range.first, range.second,
            []( const std::pair<harvest_id, season_type> &pr ) {
                if( pr.second == season_of_year( calendar::turn ) ) {
                    return "<good>" + calendar::name_season( pr.second ) + "</good>";
                }

                return "<dark>" + calendar::name_season( pr.second ) + "</dark>";
            } );
            ss << ":" << '\n';
            // List the drops
            // They actually describe what player can get from it now, so it isn't spoily
            // TODO: Allow spoily listing of everything
            ss << range.first->first.obj().describe( player_skill ) << '\n';
            // Remove the range from the multimap so that it isn't listed twice
            identical_harvest.erase( range.first, range.second );
        }

        ss << '\n';
    }

    if( deconstruct.can_do ) {
        const auto items = item_group::every_possible_item_from( deconstruct.drop_group );
        if( !items.empty() ) {
            ss << '\n' << _( "You could deconstruct it to get some of those items:" ) << '\n';
            for( const itype *it : items ) {
                ss << "<good>" << item::nname( it->get_id() ) << "</good>" << '\n';
            }
        } else {
            ss << _( "It can be deconstructed, but won't yield any resources." ) << '\n';
        }
    }

    if( debug_vision() ) {
        ss << "Bash: " << bash.str_min << "-" << bash.str_max << "\n";
    }

    if( bash.ranged && bash.ranged->reduction.min > 0 ) {
        static std::string indent = "    ";
        ss << "\n" << "Cover:" << "\n";
        if( bash.ranged->reduction.min == bash.ranged->reduction.max ) {
            ss << indent << "Damage Reduction: " << bash.ranged->reduction.min << "\n";
        } else {
            ss << indent << "Damage Reduction: " << bash.ranged->reduction.min << "-" <<
               bash.ranged->reduction.max << "\n";
        }
        if( bash.ranged->reduction_laser ) {
            if( bash.ranged->reduction_laser->min == bash.ranged->reduction_laser->max ) {
                ss << indent << "Vs. Laser: " << bash.ranged->reduction_laser->min << "\n";
            } else {
                ss << indent << "Vs. Laser: " << bash.ranged->reduction_laser->min << "-" <<
                   bash.ranged->reduction_laser->max << "\n";
            }
        }
        if( debug_vision() && bash.ranged->destroy_threshold > 0 ) {
            ss << indent << "Destroy Threshold: " << bash.ranged->destroy_threshold << "\n";
        }
        if( bash.ranged->block_unaimed_chance > 0_pct ) {
            ss << indent << "Block Chance: " << bash.ranged->block_unaimed_chance / 1_pct << "%" << "\n";
        }
        ss << "\n";
    }

    if( !flags.empty() ) {
        ss << _( "Flags: " ) << enumerate_as_string( flags.begin(), flags.end(), []( const auto & it ) {
            return it;
        } ) << '\n';
    }

    return replace_colors( ss.str() );
}
