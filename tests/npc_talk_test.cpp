#include "catch/catch.hpp"

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "avatar.h"
#include "calendar.h"
#include "character.h"
#include "character_id.h"
#include "coordinate_conversions.h"
#include "dialogue.h"
#include "effect.h"
#include "faction.h"
#include "game.h"
#include "inventory.h"
#include "item.h"
#include "item_category.h"
#include "map.h"
#include "map_helpers.h"
#include "mission.h"
#include "npc.h"
#include "npc_favor.h"
#include "npctalk.h"
#include "overmapbuffer.h"
#include "pimpl.h"
#include "player.h"
#include "player_helpers.h"
#include "point.h"
#include "state_helpers.h"
#include "string_formatter.h"
#include "string_id.h"
#include "type_id.h"

static const efftype_id effect_gave_quest_item( "gave_quest_item" );
static const efftype_id effect_currently_busy( "currently_busy" );
static const efftype_id effect_infection( "infection" );
static const efftype_id effect_infected( "infected" );

static const trait_id trait_PROF_FED( "PROF_FED" );
static const trait_id trait_PROF_SWAT( "PROF_SWAT" );

static npc &create_test_talker()
{
    const string_id<npc_template> test_talker( "test_talker" );
    const character_id model_id = get_map().place_npc( point( 25, 25 ), test_talker, true );
    g->load_npcs();

    npc *model_npc = g->find_npc( model_id );
    REQUIRE( model_npc != nullptr );

    for( const trait_id &tr : model_npc->get_mutations() ) {
        model_npc->unset_mutation( tr );
    }
    model_npc->set_stored_kcal( model_npc->max_stored_kcal() );
    model_npc->set_thirst( 0 );
    model_npc->set_fatigue( 0 );
    model_npc->remove_effect( efftype_id( "sleep" ) );
    // An ugly hack to prevent NPC falling asleep during testing due to massive fatigue
    model_npc->set_mutation( trait_id( "WEB_WEAVER" ) );

    return *model_npc;
}

static void gen_response_lines( dialogue &d, size_t expected_count )
{
    d.gen_responses( d.topic_stack.back() );
    for( talk_response &response : d.responses ) {
        response.create_option_line( d, ' ' );
    }
    if( d.responses.size() != expected_count ) {
        cata_printf( "Test failure in %s\n", d.topic_stack.back().id.c_str() );
        for( talk_response &response : d.responses ) {
            cata_printf( "response: %s\n", response.text.c_str() );
        }
    }
    CAPTURE( d.responses );
    REQUIRE( d.responses.size() == expected_count );
}

static std::string gen_dynamic_line( dialogue &d )
{
    std::string challenge = d.dynamic_line( d.topic_stack.back() );
    return challenge;
}

static void change_om_type( const std::string &new_type )
{
    // TODO: fix point types
    const tripoint_abs_omt omt_pos( ms_to_omt_copy( get_map().getabs(
                                        get_player_character().pos() ) ) );
    overmap_buffer.ter_set( omt_pos, oter_id( new_type ) );
}

static npc &prep_test( dialogue &d )
{
    clear_vehicles();
    avatar &player_character = get_avatar();
    REQUIRE_FALSE( player_character.in_vehicle );

    const tripoint test_origin( 15, 15, 0 );
    player_character.setpos( test_origin );

    g->faction_manager_ptr->create_if_needed();

    npc &talker_npc = create_test_talker();

    d.alpha = &player_character;
    d.beta = &talker_npc;

    return talker_npc;
}

TEST_CASE( "npc_talk_start", "[npc_talk]" )
{
    clear_all_state();
    dialogue d;
    prep_test( d );

    d.add_topic( "TALK_TEST_START" );
    gen_response_lines( d, 1 );
    CHECK( d.responses[0].text == "This is a basic test response." );
}

TEST_CASE( "npc_talk_describe_mission", "[npc_talk]" )
{
    clear_all_state();
    dialogue d;
    prep_test( d );

    d.add_topic( "TALK_DESCRIBE_MISSION" );
    std::string d_line = gen_dynamic_line( d );
    CHECK( d_line == "I'm looking for wounded to help." );
}

TEST_CASE( "npc_talk_stats", "[npc_talk]" )
{
    clear_all_state();
    dialogue d;
    prep_test( d );

    avatar &player_character = get_avatar();
    player_character.str_cur = 8;
    player_character.dex_cur = 8;
    player_character.int_cur = 8;
    player_character.per_cur = 8;

    d.add_topic( "TALK_TEST_SIMPLE_STATS" );
    gen_response_lines( d, 5 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    CHECK( d.responses[1].text == "This is a strength test response." );
    CHECK( d.responses[2].text == "This is a dexterity test response." );
    CHECK( d.responses[3].text == "This is an intelligence test response." );
    CHECK( d.responses[4].text == "This is a perception test response." );
    player_character.str_cur = 6;
    player_character.dex_cur = 6;
    player_character.int_cur = 6;
    player_character.per_cur = 6;
    gen_response_lines( d, 1 );
    CHECK( d.responses[0].text == "This is a basic test response." );

    d.add_topic( "TALK_TEST_NEGATED_STATS" );
    gen_response_lines( d, 5 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    CHECK( d.responses[1].text == "This is a low strength test response." );
    CHECK( d.responses[2].text == "This is a low dexterity test response." );
    CHECK( d.responses[3].text == "This is a low intelligence test response." );
    CHECK( d.responses[4].text == "This is a low perception test response." );
}

TEST_CASE( "npc_talk_skills", "[npc_talk]" )
{
    clear_all_state();
    dialogue d;
    prep_test( d );

    const skill_id skill( "driving" );

    avatar &player_character = get_avatar();
    player_character.set_skill_level( skill, 8 );

    d.add_topic( "TALK_TEST_SIMPLE_SKILLS" );
    gen_response_lines( d, 2 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    CHECK( d.responses[1].text == "This is a driving test response." );

    player_character.set_skill_level( skill, 6 );
    gen_response_lines( d, 1 );
    CHECK( d.responses[0].text == "This is a basic test response." );

    d.add_topic( "TALK_TEST_NEGATED_SKILLS" );
    gen_response_lines( d, 2 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    CHECK( d.responses[1].text == "This is a low driving test response." );
}

TEST_CASE( "npc_talk_wearing_and_trait", "[npc_talk]" )
{
    clear_all_state();
    dialogue d;
    npc &talker_npc = prep_test( d );

    avatar &player_character = get_avatar();
    for( const trait_id &tr : player_character.get_mutations() ) {
        player_character.unset_mutation( tr );
    }

    d.add_topic( "TALK_TEST_WEARING_AND_TRAIT" );
    gen_response_lines( d, 1 );

    CHECK( d.responses[0].text == "This is a basic test response." );
    player_character.toggle_trait( trait_id( "ELFA_EARS" ) );
    gen_response_lines( d, 3 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    CHECK( d.responses[1].text == "This is a trait test response." );
    CHECK( d.responses[2].text == "This is a short trait test response." );
    player_character.wear_item( item::spawn( "badge_marshal" ) );
    gen_response_lines( d, 4 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    CHECK( d.responses[1].text == "This is a trait test response." );
    CHECK( d.responses[2].text == "This is a short trait test response." );
    CHECK( d.responses[3].text == "This is a wearing test response." );
    talker_npc.toggle_trait( trait_id( "ELFA_EARS" ) );
    gen_response_lines( d, 6 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    CHECK( d.responses[1].text == "This is a trait test response." );
    CHECK( d.responses[2].text == "This is a short trait test response." );
    CHECK( d.responses[3].text == "This is a wearing test response." );
    CHECK( d.responses[4].text == "This is a npc trait test response." );
    CHECK( d.responses[5].text == "This is a npc short trait test response." );
    player_character.toggle_trait( trait_id( "ELFA_EARS" ) );
    talker_npc.toggle_trait( trait_id( "ELFA_EARS" ) );
    player_character.toggle_trait( trait_id( "PSYCHOPATH" ) );
    talker_npc.toggle_trait( trait_id( "SAPIOVORE" ) );
    gen_response_lines( d, 4 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    CHECK( d.responses[1].text == "This is a wearing test response." );
    CHECK( d.responses[2].text == "This is a trait flags test response." );
    CHECK( d.responses[3].text == "This is a npc trait flags test response." );
    player_character.toggle_trait( trait_id( "PSYCHOPATH" ) );
    talker_npc.toggle_trait( trait_id( "SAPIOVORE" ) );
}

TEST_CASE( "npc_talk_effect", "[npc_talk]" )
{
    clear_all_state();
    dialogue d;
    npc &talker_npc = prep_test( d );
    avatar &player_character = get_avatar();

    d.add_topic( "TALK_TEST_EFFECT" );
    gen_response_lines( d, 1 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    talker_npc.add_effect( effect_gave_quest_item, 9999_turns );
    gen_response_lines( d, 2 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    CHECK( d.responses[1].text == "This is an npc effect test response." );
    player_character.add_effect( effect_gave_quest_item, 9999_turns );
    d.gen_responses( d.topic_stack.back() );
    gen_response_lines( d, 3 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    CHECK( d.responses[1].text == "This is an npc effect test response." );
    CHECK( d.responses[2].text == "This is a player effect test response." );
}

TEST_CASE( "npc_talk_service", "[npc_talk]" )
{
    clear_all_state();
    dialogue d;
    npc &talker_npc = prep_test( d );
    avatar &player_character = get_avatar();

    d.add_topic( "TALK_TEST_SERVICE" );
    player_character.cash = 0;
    talker_npc.add_effect( effect_currently_busy, 9999_turns );
    gen_response_lines( d, 1 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    player_character.cash = 800;
    gen_response_lines( d, 2 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    CHECK( d.responses[1].text == "This is a cash test response." );
    talker_npc.remove_effect( effect_currently_busy );
    gen_response_lines( d, 3 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    CHECK( d.responses[1].text == "This is a cash test response." );
    CHECK( d.responses[2].text == "This is an npc available test response." );
}

TEST_CASE( "npc_talk_location", "[npc_talk]" )
{
    clear_all_state();
    dialogue d;
    prep_test( d );

    change_om_type( "pond_swamp_north" );
    d.add_topic( "TALK_TEST_LOCATION" );
    d.gen_responses( d.topic_stack.back() );
    gen_response_lines( d, 1 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    change_om_type( "field" );
    gen_response_lines( d, 2 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    CHECK( d.responses[1].text == "This is a om_location_field test response." );
}

TEST_CASE( "npc_talk_role", "[npc_talk]" )
{
    clear_all_state();
    dialogue d;
    npc &talker_npc = prep_test( d );

    d.add_topic( "TALK_TEST_NPC_ROLE" );
    talker_npc.companion_mission_role_id = "NO_TEST_ROLE";
    gen_response_lines( d, 1 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    talker_npc.companion_mission_role_id = "TEST_ROLE";
    gen_response_lines( d, 2 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    CHECK( d.responses[1].text == "This is a nearby role test response." );
}

TEST_CASE( "npc_talk_class", "[npc_talk]" )
{
    clear_all_state();
    dialogue d;
    npc &talker_npc = prep_test( d );

    d.add_topic( "TALK_TEST_NPC_CLASS" );
    talker_npc.myclass = npc_class_id( "NC_NONE" );
    gen_response_lines( d, 1 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    talker_npc.myclass = npc_class_id( "NC_TEST_CLASS" );
    gen_response_lines( d, 2 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    CHECK( d.responses[1].text == "This is a class test response." );
}

TEST_CASE( "npc_talk_allies", "[npc_talk]" )
{
    clear_all_state();
    dialogue d;
    npc &talker_npc = prep_test( d );

    for( npc *guy : g->allies() ) {
        talk_function::leave( *guy );
    }
    talk_function::follow( talker_npc );
    d.add_topic( "TALK_TEST_NPC_ALLIES" );
    gen_response_lines( d, 2 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    CHECK( d.responses[1].text == "This is a npc allies 1 test response." );
}

TEST_CASE( "npc_talk_rules", "[npc_talk]" )
{
    clear_all_state();
    dialogue d;
    npc &talker_npc = prep_test( d );

    d.add_topic( "TALK_TEST_NPC_RULES" );
    gen_response_lines( d, 1 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    talker_npc.rules.engagement = combat_engagement::ALL;
    talker_npc.rules.aim = aim_rule::SPRAY;
    talker_npc.rules.set_flag( ally_rule::use_silent );
    gen_response_lines( d, 4 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    CHECK( d.responses[1].text == "This is a npc engagement rule test response." );
    CHECK( d.responses[2].text == "This is a npc aim rule test response." );
    CHECK( d.responses[3].text == "This is a npc rule test response." );
    talker_npc.rules.clear_flag( ally_rule::use_silent );
}

TEST_CASE( "npc_talk_needs", "[npc_talk]" )
{
    clear_all_state();
    dialogue d;
    npc &talker_npc = prep_test( d );

    d.add_topic( "TALK_TEST_NPC_NEEDS" );
    gen_response_lines( d, 1 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    talker_npc.set_thirst( 90 );
    talker_npc.set_stored_kcal( 100 );
    talker_npc.set_fatigue( static_cast<int>( fatigue_levels::exhausted ) );
    gen_response_lines( d, 4 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    CHECK( d.responses[1].text == "This is a npc thirst test response." );
    CHECK( d.responses[2].text == "This is a npc hunger test response." );
    CHECK( d.responses[3].text == "This is a npc fatigue test response." );
}

TEST_CASE( "npc_talk_mission_goal", "[npc_talk]" )
{
    clear_all_state();
    dialogue d;
    npc &talker_npc = prep_test( d );

    d.add_topic( "TALK_TEST_MISSION_GOAL" );
    gen_response_lines( d, 1 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    const std::vector<mission_type> &all_missions = mission_type::get_all();
    bool set_mission = false;
    for( const mission_type &some_mission : all_missions ) {
        if( some_mission.goal == MGOAL_ASSASSINATE ) {
            mission *assassinate = mission::reserve_new( some_mission.id, talker_npc.getID() );
            talker_npc.chatbin.mission_selected = assassinate;
            set_mission = true;
            break;
        }
    }
    CHECK( set_mission );
    gen_response_lines( d, 2 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    CHECK( d.responses[1].text == "This is a mission goal test response." );
}

TEST_CASE( "npc_talk_season", "[npc_talk]" )
{
    clear_all_state();
    dialogue d;
    prep_test( d );

    const time_point old_calendar = calendar::turn;
    calendar::turn = calendar::start_of_cataclysm;
    d.add_topic( "TALK_TEST_SEASON" );
    gen_response_lines( d, 2 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    CHECK( d.responses[1].text == "This is a season spring test response." );
    calendar::turn += calendar::season_length();
    gen_response_lines( d, 4 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    CHECK( d.responses[1].text == "This is a days since Cataclysm 5 test response." );
    CHECK( d.responses[2].text == "This is a season summer test response." );
    CHECK( d.responses[3].text == "This is a days since Cataclysm 20 test response." );
    calendar::turn += calendar::season_length();
    gen_response_lines( d, 6 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    CHECK( d.responses[1].text == "This is a days since Cataclysm 5 test response." );
    CHECK( d.responses[2].text == "This is a days since Cataclysm 20 test response." );
    CHECK( d.responses[3].text == "This is a season autumn test response." );
    CHECK( d.responses[4].text == "This is a days since Cataclysm 35 test response." );
    CHECK( d.responses[5].text == "This is a days since Cataclysm 50 test response." );
    calendar::turn += calendar::season_length();
    gen_response_lines( d, 6 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    CHECK( d.responses[1].text == "This is a days since Cataclysm 5 test response." );
    CHECK( d.responses[2].text == "This is a days since Cataclysm 20 test response." );
    CHECK( d.responses[3].text == "This is a days since Cataclysm 35 test response." );
    CHECK( d.responses[4].text == "This is a season winter test response." );
    CHECK( d.responses[5].text == "This is a days since Cataclysm 50 test response." );
    calendar::turn += calendar::season_length();
    gen_response_lines( d, 6 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    CHECK( d.responses[1].text == "This is a season spring test response." );
    CHECK( d.responses[2].text == "This is a days since Cataclysm 5 test response." );
    CHECK( d.responses[3].text == "This is a days since Cataclysm 20 test response." );
    CHECK( d.responses[4].text == "This is a days since Cataclysm 35 test response." );
    CHECK( d.responses[5].text == "This is a days since Cataclysm 50 test response." );
    calendar::turn = old_calendar;
}

TEST_CASE( "npc_talk_time", "[npc_talk]" )
{
    clear_all_state();
    dialogue d;
    prep_test( d );

    const time_point old_calendar = calendar::turn;
    calendar::turn = sunrise( calendar::turn ) + 4_hours;
    d.add_topic( "TALK_TEST_TIME" );
    gen_response_lines( d, 2 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    CHECK( d.responses[1].text == "This is a is day test response." );
    calendar::turn = sunset( calendar::turn ) + 2_hours;
    gen_response_lines( d, 2 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    CHECK( d.responses[1].text == "This is a is night test response." );
    calendar::turn = old_calendar;
}

TEST_CASE( "npc_talk_switch", "[npc_talk]" )
{
    clear_all_state();
    dialogue d;
    prep_test( d );
    avatar &player_character = get_avatar();

    d.add_topic( "TALK_TEST_SWITCH" );
    player_character.cash = 1000;
    gen_response_lines( d, 3 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    CHECK( d.responses[1].text == "This is an switch 1 test response." );
    CHECK( d.responses[2].text == "This is another basic test response." );
    player_character.cash = 100;
    gen_response_lines( d, 3 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    CHECK( d.responses[1].text == "This is an switch 2 test response." );
    CHECK( d.responses[2].text == "This is another basic test response." );
    player_character.cash = 10;
    gen_response_lines( d, 4 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    CHECK( d.responses[1].text == "This is an switch default 1 test response." );
    CHECK( d.responses[2].text == "This is an switch default 2 test response." );
    CHECK( d.responses[3].text == "This is another basic test response." );
    player_character.cash = 0;
    gen_response_lines( d, 3 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    CHECK( d.responses[1].text == "This is an switch default 2 test response." );
    CHECK( d.responses[2].text == "This is another basic test response." );
}

TEST_CASE( "npc_talk_or", "[npc_talk]" )
{
    clear_all_state();
    dialogue d;
    npc &talker_npc = prep_test( d );
    avatar &player_character = get_avatar();

    d.add_topic( "TALK_TEST_OR" );
    player_character.cash = 0;
    talker_npc.add_effect( effect_currently_busy, 9999_turns );
    gen_response_lines( d, 1 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    player_character.toggle_trait( trait_id( "ELFA_EARS" ) );
    gen_response_lines( d, 2 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    CHECK( d.responses[1].text == "This is an or trait test response." );
}

TEST_CASE( "npc_talk_and", "[npc_talk]" )
{
    clear_all_state();
    dialogue d;
    npc &talker_npc = prep_test( d );
    avatar &player_character = get_avatar();

    player_character.toggle_trait( trait_id( "ELFA_EARS" ) );
    d.add_topic( "TALK_TEST_AND" );
    gen_response_lines( d, 1 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    player_character.cash = 800;
    talker_npc.remove_effect( effect_currently_busy );
    gen_response_lines( d, 2 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    CHECK( d.responses[1].text == "This is an and cash, available, trait test response." );
}

TEST_CASE( "npc_talk_nested", "[npc_talk]" )
{
    clear_all_state();
    dialogue d;
    npc &talker_npc = prep_test( d );
    avatar &player_character = get_avatar();

    d.add_topic( "TALK_TEST_NESTED" );
    talker_npc.add_effect( effect_currently_busy, 9999_turns );
    player_character.cash = 0;
    gen_response_lines( d, 1 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    player_character.cash = 800;
    player_character.int_cur = 11;
    gen_response_lines( d, 2 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    CHECK( d.responses[1].text == "This is a complex nested test response." );
}

TEST_CASE( "npc_talk_conditionals", "[npc_talk]" )
{
    clear_all_state();
    dialogue d;
    avatar &player_character = get_avatar();
    prep_test( d );
    player_character.cash = 800;

    d.add_topic( "TALK_TEST_TRUE_FALSE_CONDITIONAL" );
    gen_response_lines( d, 3 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    CHECK( d.responses[1].text == "This is a true/false true response." );
    CHECK( d.responses[2].text == "This is a conditional trial response." );
    talk_response &chosen = d.responses[2];
    bool trial_success = chosen.trial.roll( d );
    CHECK( trial_success == true );
    talk_effect_t &trial_effect = trial_success ? chosen.success : chosen.failure;
    CHECK( trial_effect.next_topic.id == "TALK_TEST_TRUE_CONDITION_NEXT" );
    player_character.cash = 0;
    gen_response_lines( d, 3 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    CHECK( d.responses[1].text == "This is a true/false false response." );
    CHECK( d.responses[2].text == "This is a conditional trial response." );
    chosen = d.responses[2];
    trial_success = chosen.trial.roll( d );
    CHECK( trial_success == false );
    trial_effect = trial_success ? chosen.success : chosen.failure;
    CHECK( trial_effect.next_topic.id == "TALK_TEST_FALSE_CONDITION_NEXT" );
}

static bool has_item( Character &p, const std::string &id, int count )
{
    item &old_item = *item::spawn_temporary( id );
    if( old_item.count_by_charges() ) {
        return p.has_charges( itype_id( id ), count );
    } else {
        return p.has_amount( itype_id( id ), count );
    }
}

static bool has_beer_bottle( Character &p, int count )
{
    return has_item( p, "bottle_glass", 1 ) && has_item( p, "beer", count );
}

static void give_item( Character &p, const std::string &id, int count )
{
    for( int i = 0; i < count; i++ ) {
        p.i_add( item::spawn( id ) );
    }
}

TEST_CASE( "npc_talk_effects_advanced", "[npc_talk]" )
{
    clear_all_state();
    dialogue d;
    npc &talker_npc = prep_test( d );
    avatar &player_character = get_avatar();

    player_character.int_cur = 8;
    player_character.cash = 1000;
    player_character.remove_top_items_with( []( detached_ptr<item> &&it ) {
        return ( it->get_category().get_id() == item_category_id( "books" ) ||
                 it->get_category().get_id() == item_category_id( "food" ) ||
                 it->get_category().get_id() == item_category_id( "drugs" ) ||
                 it->typeId() == itype_id( "bottle_glass" ) ) ? detached_ptr<item>() : std::move( it );
    } );

    SECTION( "basic_has_item" ) {
        d.add_topic( "TALK_TEST_HAS_ITEM" );
        gen_response_lines( d, 1 );
        CHECK( d.responses[0].text == "This is a basic test response." );
    }

    SECTION( "basic_item_repeat" ) {
        d.add_topic( "TALK_TEST_ITEM_REPEAT" );
        gen_response_lines( d, 1 );
        CHECK( d.responses[0].text == "This is a basic test response." );
    }

    SECTION( "add_remove_effect" ) {
        d.add_topic( "TALK_TEST_EFFECTS_CHAREFFECTS" );
        gen_response_lines( d, 5 );
        // add and remove effect
        REQUIRE_FALSE( player_character.has_effect( effect_infection ) );
        talk_effect_t &effects = d.responses[1].success;
        effects.apply( d );
        CHECK( player_character.has_effect( effect_infection ) );
        CHECK( player_character.get_effect_dur( effect_infection ) == time_duration::from_turns( 10 ) );
        REQUIRE_FALSE( talker_npc.has_effect( effect_infection ) );
        effects = d.responses[2].success;
        effects.apply( d );
        CHECK( talker_npc.has_effect( effect_infection ) );
        CHECK( talker_npc.get_effect( effect_infection ).is_permanent() );
        effects = d.responses[3].success;
        effects.apply( d );
        CHECK_FALSE( player_character.has_effect( effect_infection ) );
        effects = d.responses[4].success;
        effects.apply( d );
        CHECK_FALSE( talker_npc.has_effect( effect_infection ) );
    }

    SECTION( "add_remove_trait" ) {
        d.add_topic( "TALK_TEST_EFFECTS_CHARTRAIT" );
        gen_response_lines( d, 5 );
        // add and remove trait
        REQUIRE_FALSE( player_character.has_trait( trait_PROF_FED ) );
        talk_effect_t &effects = d.responses[1].success;
        effects.apply( d );
        CHECK( player_character.has_trait( trait_PROF_FED ) );
        REQUIRE_FALSE( talker_npc.has_trait( trait_PROF_FED ) );
        effects = d.responses[2].success;
        effects.apply( d );
        CHECK( talker_npc.has_trait( trait_PROF_FED ) );
        effects = d.responses[3].success;
        effects.apply( d );
        CHECK_FALSE( player_character.has_trait( trait_PROF_FED ) );
        effects = d.responses[4].success;
        effects.apply( d );
        CHECK_FALSE( talker_npc.has_trait( trait_PROF_FED ) );
    }

    SECTION( "buy_and_spend" ) {
        d.add_topic( "TALK_TEST_EFFECTS_BUY_SPEND" );
        gen_response_lines( d, 4 );
        // buying and spending
        talker_npc.op_of_u.owed = 1000;
        REQUIRE_FALSE( has_beer_bottle( player_character, 2 ) );
        REQUIRE( talker_npc.op_of_u.owed == 1000 );
        REQUIRE( player_character.cash == 1000 );
        talk_effect_t &effects = d.responses[1].success;
        effects.apply( d );
        CHECK( talker_npc.op_of_u.owed == 500 );
        CHECK( has_beer_bottle( player_character, 2 ) );
        REQUIRE_FALSE( has_item( player_character, "bottle_plastic", 1 ) );
        effects = d.responses[2].success;
        effects.apply( d );
        CHECK( has_item( player_character, "bottle_plastic", 1 ) );
        // No price specified means gift
        CHECK( talker_npc.op_of_u.owed == 500 );
        effects = d.responses[3].success;
        effects.apply( d );
        // Ecash should NOT be used for trade or to settle a debt
        CHECK( player_character.cash == 500 );
        CHECK( talker_npc.op_of_u.owed == 500 );
    }

    SECTION( "effect_chains" ) {
        d.add_topic( "TALK_TEST_EFFECTS_CHAIN" );
        gen_response_lines( d, 2 );
        // effect chains
        REQUIRE_FALSE( player_character.has_effect( effect_infected ) );
        REQUIRE_FALSE( talker_npc.has_effect( effect_infected ) );
        REQUIRE_FALSE( player_character.has_trait( trait_PROF_SWAT ) );
        REQUIRE_FALSE( talker_npc.has_trait( trait_PROF_SWAT ) );
        talk_effect_t &effects = d.responses[1].success;
        effects.apply( d );
        CHECK( player_character.has_effect( effect_infected ) );
        CHECK( player_character.get_effect_dur( effect_infected ) == time_duration::from_turns( 10 ) );
        CHECK( talker_npc.has_effect( effect_infected ) );
        CHECK( talker_npc.get_effect( effect_infected ).is_permanent() );
        CHECK( player_character.has_trait( trait_PROF_SWAT ) );
        CHECK( talker_npc.has_trait( trait_PROF_SWAT ) );
        CHECK( talker_npc.op_of_u.owed == 0 );
        CHECK( talker_npc.get_attitude() == NPCATT_KILL );
    }

    SECTION( "opinion" ) {
        d.add_topic( "TALK_TEST_EFFECTS_OPINION" );
        gen_response_lines( d, 2 );
        // opinion changes
        talker_npc.op_of_u = npc_opinion();
        REQUIRE_FALSE( talker_npc.op_of_u.trust );
        REQUIRE_FALSE( talker_npc.op_of_u.fear );
        REQUIRE_FALSE( talker_npc.op_of_u.value );
        REQUIRE_FALSE( talker_npc.op_of_u.anger );
        REQUIRE_FALSE( talker_npc.op_of_u.owed );
        talk_effect_t &effects = d.responses[1].success;
        REQUIRE( effects.opinion.trust == 10 );
        effects.apply( d );
        CHECK( talker_npc.op_of_u.trust == 10 );
        CHECK( talker_npc.op_of_u.fear == 11 );
        CHECK( talker_npc.op_of_u.value == 12 );
        CHECK( talker_npc.op_of_u.anger == 13 );
        CHECK( talker_npc.op_of_u.owed == 14 );
    }

    SECTION( "has_item" ) {
        give_item( player_character, "beer", 2 );
        give_item( player_character, "bottle_glass", 1 );
        give_item( player_character, "manual_speech", 1 );
        give_item( player_character, "dnd_handbook", 1 );

        d.add_topic( "TALK_TEST_HAS_ITEM" );
        gen_response_lines( d, 7 );
        CHECK( d.responses[0].text == "This is a repeated item manual_speech test response" );
        CHECK( d.responses[0].success.next_topic.item_type == itype_id( "manual_speech" ) );
        CHECK( d.responses[1].text == "This is a basic test response." );
        CHECK( d.responses[2].text == "This is a u_has_item beer test response." );
        CHECK( d.responses[3].text == "This is a u_has_item bottle_glass test response." );
        CHECK( d.responses[4].text == "This is a u_has_items beer test response." );
        CHECK( d.responses[5].text == "This is a u_has_item_category books test response." );
        CHECK( d.responses[6].text == "This is a u_has_item_category books count 2 test response." );
    }

    SECTION( "item_repeat" ) {
        give_item( player_character, "beer", 2 );
        give_item( player_character, "bottle_glass", 1 );
        give_item( player_character, "manual_speech", 1 );
        give_item( player_character, "dnd_handbook", 1 );

        d.add_topic( "TALK_TEST_ITEM_REPEAT" );
        gen_response_lines( d, 8 );
        CHECK( d.responses[0].text == "This is a repeated category books, food test response" );
        CHECK( d.responses[0].success.next_topic.item_type == itype_id( "beer" ) );
        CHECK( d.responses[1].text == "This is a repeated category books, food test response" );
        CHECK( d.responses[1].success.next_topic.item_type == itype_id( "dnd_handbook" ) );
        CHECK( d.responses[2].text == "This is a repeated category books, food test response" );
        CHECK( d.responses[2].success.next_topic.item_type == itype_id( "manual_speech" ) );
        CHECK( d.responses[3].text == "This is a repeated category books test response" );
        CHECK( d.responses[3].success.next_topic.item_type == itype_id( "dnd_handbook" ) );
        CHECK( d.responses[4].text == "This is a repeated category books test response" );
        CHECK( d.responses[4].success.next_topic.item_type == itype_id( "manual_speech" ) );
        CHECK( d.responses[5].text == "This is a repeated item beer, bottle_glass test response" );
        CHECK( d.responses[5].success.next_topic.item_type == itype_id( "bottle_glass" ) );
        CHECK( d.responses[6].text == "This is a repeated item beer, bottle_glass test response" );
        CHECK( d.responses[6].success.next_topic.item_type == itype_id( "beer" ) );
        CHECK( d.responses[7].text == "This is a basic test response." );
    }

    SECTION( "sell_and_consume" ) {
        give_item( player_character, "beer", 2 );
        give_item( player_character, "bottle_glass", 1 );
        give_item( player_character, "bottle_plastic", 1 );

        // test sell and consume
        d.add_topic( "TALK_TEST_EFFECTS_SELL_CONSUME" );
        gen_response_lines( d, 5 );
        REQUIRE( player_character.cash == 1000 );
        REQUIRE( has_item( player_character, "bottle_plastic", 1 ) );
        REQUIRE( has_beer_bottle( player_character, 2 ) );
        REQUIRE( player_character.wield( player_character.i_at( player_character.inv_position_by_type(
                                             itype_id( "bottle_glass" ) ) ) ) );
        talk_effect_t &effects = d.responses[1].success;
        effects.apply( d );
        CHECK_FALSE( has_item( player_character, "bottle_plastic", 1 ) );
        CHECK_FALSE( has_item( player_character, "beer", 1 ) );
        CHECK( has_item( talker_npc, "bottle_plastic", 1 ) );
        CHECK( has_item( talker_npc, "beer", 2 ) );
        effects = d.responses[2].success;
        effects.apply( d );
        CHECK_FALSE( has_item( talker_npc, "beer", 2 ) );
        CHECK( has_item( talker_npc, "beer", 1 ) );
        effects = d.responses[3].success;
        effects.apply( d );
        CHECK( has_item( player_character, "beer", 1 ) );
        effects = d.responses[4].success;
        effects.apply( d );
        CHECK( has_item( player_character, "beer", 0 ) );
        CHECK_FALSE( has_item( player_character, "beer", 1 ) );
        // Ecash should NOT be used for trade
        REQUIRE( player_character.cash == 1000 );
    }
}

TEST_CASE( "npc_talk_combat_commands", "[npc_talk]" )
{
    clear_all_state();
    dialogue d;
    prep_test( d );

    d.add_topic( "TALK_COMBAT_COMMANDS" );
    gen_response_lines( d, 10 );
    CHECK( d.responses[0].text == "Change your engagement rules…" );
    CHECK( d.responses[1].text == "Change your aiming rules…" );
    CHECK( d.responses[2].text == "Stick close to me, no matter what." );
    CHECK( d.responses[3].text == "Follow me about four paces away." );
    CHECK( d.responses[4].text == "Don't use ranged weapons anymore." );
    CHECK( d.responses[5].text == "Use only silent weapons." );
    CHECK( d.responses[6].text == "Don't use grenades anymore." );
    CHECK( d.responses[7].text == "Don't worry about shooting an ally." );
    CHECK( d.responses[8].text == "Hold the line: don't move onto obstacles adjacent to me." );
    CHECK( d.responses[9].text == "Never mind." );
}

TEST_CASE( "npc_talk_vars", "[npc_talk]" )
{
    clear_all_state();
    dialogue d;
    prep_test( d );

    d.add_topic( "TALK_TEST_VARS" );
    gen_response_lines( d, 3 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    CHECK( d.responses[1].text == "This is a u_add_var test response." );
    CHECK( d.responses[2].text == "This is a npc_add_var test response." );
    talk_effect_t &effects = d.responses[1].success;
    effects.apply( d );
    effects = d.responses[2].success;
    effects.apply( d );
    gen_response_lines( d, 3 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    CHECK( d.responses[1].text == "This is a u_has_var, u_remove_var test response." );
    CHECK( d.responses[2].text == "This is a npc_has_var, npc_remove_var test response." );
    effects = d.responses[1].success;
    effects.apply( d );
    effects = d.responses[2].success;
    effects.apply( d );
    gen_response_lines( d, 3 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    CHECK( d.responses[1].text == "This is a u_add_var test response." );
    CHECK( d.responses[2].text == "This is a npc_add_var test response." );
}

TEST_CASE( "npc_talk_adjust_vars", "[npc_talk]" )
{
    clear_all_state();
    dialogue d;
    prep_test( d );

    d.add_topic( "TALK_TEST_ADJUST_VARS" );

    // At the starting point, the var hasn't been set or adjusted, so it should default to 0.
    gen_response_lines( d, 11 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    CHECK( d.responses[1].text == "This is a u_adjust_var test response that increments by 1." );
    CHECK( d.responses[2].text == "This is a u_adjust_var test response that decrements by 1." );
    CHECK( d.responses[3].text == "This is a npc_adjust_var test response that increments by 1." );
    CHECK( d.responses[4].text == "This is a npc_adjust_var test response that decrements by 1." );
    CHECK( d.responses[5].text == "This is a u_compare_var test response for == 0." );
    CHECK( d.responses[6].text == "This is a u_compare_var test response for <= 0." );
    CHECK( d.responses[7].text == "This is a u_compare_var test response for >= 0." );
    CHECK( d.responses[8].text == "This is a npc_compare_var test response for == 0." );
    CHECK( d.responses[9].text == "This is a npc_compare_var test response for <= 0." );
    CHECK( d.responses[10].text == "This is a npc_compare_var test response for >= 0." );

    // Increment the u and npc vars by 1, so that it has a value of 1.
    talk_effect_t &effects = d.responses[1].success;
    effects.apply( d );
    effects = d.responses[3].success;
    effects.apply( d );

    // Now we're comparing the var, which should be 1, to our condition value which is 0.
    gen_response_lines( d, 11 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    CHECK( d.responses[1].text == "This is a u_adjust_var test response that increments by 1." );
    CHECK( d.responses[2].text == "This is a u_adjust_var test response that decrements by 1." );
    CHECK( d.responses[3].text == "This is a npc_adjust_var test response that increments by 1." );
    CHECK( d.responses[4].text == "This is a npc_adjust_var test response that decrements by 1." );
    CHECK( d.responses[5].text == "This is a u_compare_var test response for != 0." );
    CHECK( d.responses[6].text == "This is a u_compare_var test response for >= 0." );
    CHECK( d.responses[7].text == "This is a u_compare_var test response for > 0." );
    CHECK( d.responses[8].text == "This is a npc_compare_var test response for != 0." );
    CHECK( d.responses[9].text == "This is a npc_compare_var test response for >= 0." );
    CHECK( d.responses[10].text == "This is a npc_compare_var test response for > 0." );

    // Decrement the u and npc vars by 1 twice, so that it has a value of -1.
    effects = d.responses[2].success;
    effects.apply( d );
    effects.apply( d );
    effects = d.responses[4].success;
    effects.apply( d );
    effects.apply( d );

    // Now we're comparing the var, which should be -1, to our condition value which is 0.
    gen_response_lines( d, 11 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    CHECK( d.responses[1].text == "This is a u_adjust_var test response that increments by 1." );
    CHECK( d.responses[2].text == "This is a u_adjust_var test response that decrements by 1." );
    CHECK( d.responses[3].text == "This is a npc_adjust_var test response that increments by 1." );
    CHECK( d.responses[4].text == "This is a npc_adjust_var test response that decrements by 1." );
    CHECK( d.responses[5].text == "This is a u_compare_var test response for != 0." );
    CHECK( d.responses[6].text == "This is a u_compare_var test response for <= 0." );
    CHECK( d.responses[7].text == "This is a u_compare_var test response for < 0." );
    CHECK( d.responses[8].text == "This is a npc_compare_var test response for != 0." );
    CHECK( d.responses[9].text == "This is a npc_compare_var test response for <= 0." );
    CHECK( d.responses[10].text == "This is a npc_compare_var test response for < 0." );
}

TEST_CASE( "npc_talk_bionics", "[npc_talk]" )
{
    clear_all_state();
    dialogue d;
    npc &talker_npc = prep_test( d );
    avatar &player_character = get_avatar();

    player_character.clear_bionics();
    talker_npc.clear_bionics();
    d.add_topic( "TALK_TEST_BIONICS" );
    gen_response_lines( d, 1 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    player_character.add_bionic( bionic_id( "bio_ads" ) );
    talker_npc.add_bionic( bionic_id( "bio_power_storage" ) );
    gen_response_lines( d, 3 );
    CHECK( d.responses[0].text == "This is a basic test response." );
    CHECK( d.responses[1].text == "This is a u_has_bionics bio_ads test response." );
    CHECK( d.responses[2].text == "This is a npc_has_bionics ANY response." );
}

TEST_CASE( "npc_talk_effects", "[npc_talk]" )
{
    clear_all_state();
    dialogue d;
    npc &talker_npc = prep_test( d );
    avatar &player_character = get_avatar();

    // Speaker effects test use cash for simplicity
    player_character.cash = 10000;
    CHECK( player_character.cash == 10000 );

    SECTION( "simple" ) {
        d.add_topic( "TALK_TEST_SPEAKER_EFFECT_SIMPLE" );
        d.apply_speaker_effects( d.topic_stack.back() );
        REQUIRE( player_character.cash == 9500 );
        d.apply_speaker_effects( d.topic_stack.back() );
        REQUIRE( player_character.cash == 9000 );

        SECTION( "simple_conditional" ) {
            d.add_topic( "TALK_TEST_SPEAKER_EFFECT_SIMPLE_CONDITIONAL" );
            d.apply_speaker_effects( d.topic_stack.back() );
            REQUIRE( player_character.cash == 8500 );
            d.apply_speaker_effects( d.topic_stack.back() );
            REQUIRE( player_character.cash == 8500 );
        }
    }

    SECTION( "sentinel" ) {
        d.add_topic( "TALK_TEST_SPEAKER_EFFECT_SENTINEL" );
        d.apply_speaker_effects( d.topic_stack.back() );
        REQUIRE( player_character.cash == 9500 );
        d.apply_speaker_effects( d.topic_stack.back() );
        REQUIRE( player_character.cash == 9500 );

        SECTION( "sentinel_conditional" ) {
            d.add_topic( "TALK_TEST_SPEAKER_EFFECT_SENTINEL_CONDITIONAL" );
            d.apply_speaker_effects( d.topic_stack.back() );
            REQUIRE( player_character.cash == 9000 );
            d.apply_speaker_effects( d.topic_stack.back() );
            REQUIRE( player_character.cash == 9000 );
        }
    }

    SECTION( "compound" ) {
        d.add_topic( "TALK_TEST_SPEAKER_EFFECT_COMPOUND" );
        d.apply_speaker_effects( d.topic_stack.back() );
        REQUIRE( player_character.cash == 9000 );
        d.apply_speaker_effects( d.topic_stack.back() );
        REQUIRE( player_character.cash == 8000 );

        SECTION( "compound_conditional" ) {
            d.add_topic( "TALK_TEST_SPEAKER_EFFECT_COMPOUND_CONDITIONAL" );
            d.apply_speaker_effects( d.topic_stack.back() );
            REQUIRE( player_character.cash == 6500 );
            d.apply_speaker_effects( d.topic_stack.back() );
            REQUIRE( player_character.cash == 6000 );
        }
    }

    SECTION( "compound_sentinel" ) {
        d.add_topic( "TALK_TEST_SPEAKER_EFFECT_COMPOUND_SENTINEL" );
        d.apply_speaker_effects( d.topic_stack.back() );
        REQUIRE( player_character.cash == 9250 );
        d.apply_speaker_effects( d.topic_stack.back() );
        REQUIRE( player_character.cash == 8750 );

        SECTION( "sentinel_conditional" ) {
            d.add_topic( "TALK_TEST_SPEAKER_EFFECT_COMPOUND_SENTINEL_CONDITIONAL" );
            d.apply_speaker_effects( d.topic_stack.back() );
            REQUIRE( player_character.cash == 8000 );
            d.apply_speaker_effects( d.topic_stack.back() );
            REQUIRE( player_character.cash == 8000 );
        }
    }

    SECTION( "change_npc_class" ) {
        // test change class
        talker_npc.myclass = npc_class_id( "NC_TEST_CLASS" );
        d.add_topic( "TALK_TEST_EFFECTS_NPC_CLASS" );
        gen_response_lines( d, 2 );
        talk_effect_t &effects = d.responses[1].success;
        effects.apply( d );
        CHECK( talker_npc.myclass == npc_class_id( "NC_NONE" ) );
    }
}

TEST_CASE( "npc_talk_effect_change_first_topic", "[npc_talk]" )
{
    clear_all_state();
    dialogue d;
    npc &talker_npc = prep_test( d );

    d.add_topic( "TALK_TEST_CHANGE_FIRST_TOPIC_1" );
    gen_response_lines( d, 2 );
    talk_effect_t &effect_1 = d.responses[1].success;
    effect_1.apply( d );
    CHECK( talker_npc.chatbin.first_topic == "TALK_TEST_CHANGE_FIRST_TOPIC_2" );
    d.add_topic( "TALK_TEST_CHANGE_FIRST_TOPIC_2" );
    gen_response_lines( d, 2 );
    talk_effect_t &effect_2 = d.responses[1].success;
    effect_2.apply( d );
    CHECK( talker_npc.chatbin.first_topic == "TALK_TEST_CHANGE_FIRST_TOPIC_1" );
}
