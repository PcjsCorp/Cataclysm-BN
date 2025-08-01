#include "gates.h"

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "activity_actor.h"
#include "activity_actor_definitions.h"
#include "avatar.h"
#include "character.h"
#include "creature.h"
#include "debug.h"
#include "enums.h"
#include "game.h" // TODO: This is a circular dependency
#include "generic_factory.h"
#include "iexamine.h"
#include "int_id.h"
#include "item.h"
#include "json.h"
#include "map.h"
#include "mapdata.h"
#include "messages.h"
#include "player.h"
#include "player_activity.h"
#include "point.h"
#include "string_id.h"
#include "translations.h"
#include "type_id.h"
#include "units.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "vpart_position.h"

// Gates namespace

namespace
{

struct gate_data;

using gate_id = string_id<gate_data>;

struct gate_data {

    gate_data() :
        moves( 0 ),
        bash_dmg( 0 ),
        was_loaded( false ) {}

    gate_id id;

    ter_str_id door;
    ter_str_id floor;
    std::vector<ter_str_id> walls;

    translation pull_message;
    translation open_message;
    translation close_message;
    translation fail_message;

    int moves;
    int bash_dmg;
    bool was_loaded;

    void load( const JsonObject &jo, const std::string &src );
    void check() const;

    bool is_suitable_wall( const tripoint &pos ) const;
};

gate_id get_gate_id( const tripoint &pos )
{
    return gate_id( get_map().ter( pos ).id().str() );
}

generic_factory<gate_data> gates_data( "gate type" );

} // namespace

void gate_data::load( const JsonObject &jo, const std::string & )
{
    mandatory( jo, was_loaded, "door", door );
    mandatory( jo, was_loaded, "floor", floor );
    mandatory( jo, was_loaded, "walls", walls, string_id_reader<ter_t> {} );

    if( !was_loaded || jo.has_member( "messages" ) ) {
        JsonObject messages_obj = jo.get_object( "messages" );

        optional( messages_obj, was_loaded, "pull", pull_message );
        optional( messages_obj, was_loaded, "open", open_message );
        optional( messages_obj, was_loaded, "close", close_message );
        optional( messages_obj, was_loaded, "fail", fail_message );
    }

    optional( jo, was_loaded, "moves", moves, 0 );
    optional( jo, was_loaded, "bashing_damage", bash_dmg, 0 );
}

void gate_data::check() const
{
    static const iexamine_function controls_gate( iexamine_function_from_string( "controls_gate" ) );
    const ter_str_id winch_tid( id.str() );

    if( !winch_tid.is_valid() ) {
        debugmsg( "Gates \"%s\" have no terrain of the same name, working as a winch.", id.c_str() );
    } else if( winch_tid->examine != controls_gate ) {
        debugmsg( "Terrain \"%s\" can't control gates, but gates \"%s\" depend on it.",
                  winch_tid.c_str(), id.c_str() );
    }

    if( !door.is_valid() ) {
        debugmsg( "Invalid door \"%s\" in \"%s\".", door.c_str(), id.c_str() );
    }
    if( !floor.is_valid() ) {
        debugmsg( "Invalid floor \"%s\" in \"%s\".", floor.c_str(), id.c_str() );
    }
    for( const auto &elem : walls ) {
        if( !elem.is_valid() ) {
            debugmsg( "Invalid wall \"%s\" in \"%s\".", elem.c_str(), id.c_str() );
        }
    }

    if( moves < 0 ) {
        debugmsg( "Gates \"%s\" grant moves.", id.c_str() );
    }
}

bool gate_data::is_suitable_wall( const tripoint &pos ) const
{
    const auto wid = get_map().ter( pos );
    const auto iter = std::ranges::find_if( walls, [ wid ]( const ter_str_id & wall ) {
        return wall.id() == wid;
    } );
    return iter != walls.end();
}

void gates::load( const JsonObject &jo, const std::string &src )
{
    gates_data.load( jo, src );
}

void gates::check()
{
    gates_data.check();
}

void gates::reset()
{
    gates_data.reset();
}

// A gate handle is adjacent to a wall section, and next to that wall section on one side or
// another is the gate.  There may be a handle on the other side, but this is optional.
// The gate continues until it reaches a non-floor tile, so they can be arbitrary length.
//
//   |  !|!  -++-++-  !|++++-
//   +   +      !      +
//   +   +   -++-++-   +
//   +   +             +
//   +   +   !|++++-   +
//  !|   |!        !   |
//

void gates::toggle_gate( const tripoint &pos )
{
    const gate_id gid = get_gate_id( pos );

    if( !gates_data.is_valid( gid ) ) {
        return;
    }

    const gate_data &gate = gates_data.obj( gid );

    bool open = false;
    bool close = false;
    bool fail = false;

    map &here = get_map();
    for( point wall_offset : four_adjacent_offsets ) {
        const tripoint wall_pos = pos + wall_offset;

        if( !gate.is_suitable_wall( wall_pos ) ) {
            continue;
        }

        for( point gate_offset : four_adjacent_offsets ) {
            const tripoint gate_pos = wall_pos + gate_offset;

            if( gate_pos == pos ) {
                continue; // Never comes back
            }

            if( !open ) { // Closing the gate...
                tripoint cur_pos = gate_pos;
                while( here.ter( cur_pos ) == gate.floor.id() ) {
                    fail = !g->forced_door_closing( cur_pos, gate.door.id(), gate.bash_dmg ) || fail;
                    close = !fail;
                    cur_pos += gate_offset;
                }
            }

            if( !close ) { // Opening the gate...
                tripoint cur_pos = gate_pos;
                while( true ) {
                    const ter_id ter = here.ter( cur_pos );

                    if( ter == gate.door.id() ) {
                        here.ter_set( cur_pos, gate.floor.id() );
                        open = !fail;
                    } else if( ter != gate.floor.id() ) {
                        break;
                    }
                    cur_pos += gate_offset;
                }
            }
        }
    }

    if( g->u.sees( pos ) ) {
        if( open ) {
            add_msg( gate.open_message );
        } else if( close ) {
            add_msg( gate.close_message );
        } else if( fail ) {
            add_msg( gate.fail_message );
        } else {
            add_msg( _( "Nothing happens." ) );
        }
    }
}

void gates::toggle_gate( const tripoint &pos, Character &who )
{
    const gate_id gid = get_gate_id( pos );

    if( !gates_data.is_valid( gid ) ) {
        who.add_msg_if_player( _( "Nothing happens." ) );
        return;
    }

    const gate_data &gate = gates_data.obj( gid );

    who.add_msg_if_player( gate.pull_message );
    who.assign_activity( std::make_unique<player_activity>
                         ( std::make_unique<toggle_gate_activity_actor>(
                               gate.moves,
                               pos
                           ) ) );
}

// Doors namespace

void doors::close_door( map &m, Character &who, const tripoint &closep )
{
    bool didit = false;
    const bool inside = !m.is_outside( who.pos() );

    const Creature *const mon = g->critter_at( closep );
    if( mon ) {
        if( mon->is_player() ) {
            who.add_msg_if_player( m_info, _( "There's some buffoon in the way!" ) );
        } else if( mon->is_monster() ) {
            // TODO: Houseflies, mosquitoes, etc shouldn't count
            who.add_msg_if_player( m_info, _( "%s is in the way!" ), mon->disp_name( false, true ) );
        } else {
            who.add_msg_if_player( m_info, _( "%s is in the way!" ), mon->disp_name() );
        }
        return;
    }

    if( optional_vpart_position vp = m.veh_at( closep ) ) {
        vehicle *const veh = &vp->vehicle();
        const int vpart = vp->part_index();
        const int closable = veh->next_part_to_close( vpart,
                             veh_pointer_or_null( m.veh_at( who.pos() ) ) != veh );
        const int inside_closable = veh->next_part_to_close( vpart );
        const int openable = veh->next_part_to_open( vpart );
        if( closable >= 0 ) {
            if( who.is_avatar() && !veh->handle_potential_theft( *who.as_avatar() ) ) {
                return;
            }
            Character *ch = who.as_character();
            if( ch && veh->can_close( closable, *ch ) ) {
                veh->close( closable );
                //~ %1$s - vehicle name, %2$s - part name
                who.add_msg_if_player( _( "You close the %1$s's %2$s." ), veh->name, veh->part( closable ).name() );
                didit = true;
            }
        } else if( inside_closable >= 0 ) {
            who.add_msg_if_player( m_info, _( "That %s can only be closed from the inside." ),
                                   veh->part( inside_closable ).name() );
        } else if( openable >= 0 ) {
            who.add_msg_if_player( m_info, _( "That %s is already closed." ),
                                   veh->part( openable ).name() );
        } else {
            who.add_msg_if_player( m_info, _( "You cannot close the %s." ), veh->part( vpart ).name() );
        }
    } else if( m.furn( closep ) == furn_str_id( "f_crate_o" ) ) {
        who.add_msg_if_player( m_info, _( "You'll need to construct a seal to close the crate!" ) );
    } else if( !m.close_door( closep, inside, true ) ) {
        if( m.close_door( closep, true, true ) ) {
            who.add_msg_if_player( m_info,
                                   _( "You cannot close the %s from outside.  You must be inside the building." ),
                                   m.name( closep ) );
        } else {
            who.add_msg_if_player( m_info, _( "You cannot close the %s." ), m.name( closep ) );
        }
    } else {
        auto items_in_way = m.i_at( closep );
        // Scoot up to 25 liters of items out of the way
        if( m.furn( closep ) != furn_str_id( "f_safe_o" ) && !items_in_way.empty() ) {
            const units::volume max_nudge = 25_liter;

            const auto toobig = std::ranges::find_if( items_in_way,
            [&max_nudge]( const item * const & it ) {
                return it->volume() > max_nudge;
            } );
            if( toobig != items_in_way.end() ) {
                who.add_msg_if_player( m_info, _( "The %s is too big to just nudge out of the way." ),
                                       ( *toobig )->tname() );
            } else if( items_in_way.stored_volume() > max_nudge ) {
                who.add_msg_if_player( m_info, _( "There is too much stuff in the way." ) );
            } else {
                m.close_door( closep, inside, false );
                didit = true;
                who.add_msg_if_player( m_info, _( "You push the %s out of the way." ),
                                       items_in_way.size() == 1 ? items_in_way.only_item().tname() : _( "stuff" ) );
                who.mod_moves( -std::min( items_in_way.stored_volume() / ( max_nudge / 50 ), 100 ) );

                if( m.has_flag( "NOITEM", closep ) ) {
                    // Just plopping items back on their origin square will displace them to adjacent squares
                    // since the door is closed now.

                    for( auto &elem : m.i_clear( closep ) ) {
                        m.add_item_or_charges( closep, std::move( elem ) );
                    }
                }
            }
        } else {
            m.close_door( closep, inside, false );
            didit = true;
        }
    }

    if( didit ) {
        // TODO: Vary this? Based on strength, broken legs, and so on.
        who.mod_moves( -90 );
    }
}
