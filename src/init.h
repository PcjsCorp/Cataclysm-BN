#pragma once

#include <functional>
#include <list>
#include <map>
#include <string>
#include <vector>
#include <utility>

#include "catalua.h"
#include "json.h"
#include "memory_fast.h"
#include "type_id.h"

class loading_ui;
class JsonObject;
class JsonIn;
class world;

/**
 * This class is used to load (and unload) the dynamic
 * (and moddable) data from json files.
 * There exists only one instance of this class, which
 * can be accessed with @ref get_instance
 *
 * Usage is basically this:
 * - Let user decide which world to play in.
 * - Call @ref unload_data (to unload data from a
 * previously loaded world, if any)
 * - Call @ref load_data_from_path(...) repeatedly with
 * different paths for all the mods
 * of the current world.
 * - Call @ref finalize_loaded_data when all mods have been
 * loaded.
 * - Play.
 *
 * The object initializes itself upon first usage.
 * It also unloads everything when the program ends.
 *
 *
 *
 * Porting stuff to json works like this:
 * - create a function
 *       void load_my_object(JsonObject &jo);
 * - Add an entry to @ref type_function_map (inside of @ref initialize)
 *   that calls the new function.
 * - Inside that function load the data from the json object.
 * You must also provide a reset function and add a call to
 * that function in @ref unload_data
 * - Optional: create a finalize function and call it from
 * @ref finalize_loaded_data
 * - Optional: create a function to check the consistency of
 * the loaded data and call this function from @ref check_consistency
 * - Than create json files.
 */
class DynamicDataLoader
{
    public:
        using type_string = std::string;
        using t_type_function_map =
            std::map<type_string, std::function<void( const JsonObject &, const std::string &, const std::string &, const std::string & )>>;
        using str_vec = std::vector<std::string>;

        /**
         * JSON data dependent upon as-yet unparsed definitions
         * first: JSON source location, second: source identifier
         */
        using deferred_json = std::vector<std::pair<json_source_location, std::string>>;

        std::unique_ptr<cata::lua_state, cata::lua_state_deleter> lua;

    private:
        bool finalized = false;

        struct cached_streams;
        std::unique_ptr<cached_streams> stream_cache;

        /**
         * Maps the type string (coming from json) to the
         * functor that loads that kind of object from json.
         */
        t_type_function_map type_function_map;
        void add( const std::string &type, const std::function<void( const JsonObject & )> &f );
        void add( const std::string &type,
                  const std::function<void( const JsonObject &, const std::string & )> &f );
        void add( const std::string &type,
                  std::function<void( const JsonObject &, const std::string &, const std::string &, const std::string & )>
                  f );
        /**
         * Load all the types from that json data.
         * @param jsin Might contain single object,
         * or an array of objects. Each object must have a
         * "type", that is part of the @ref type_function_map
         * @param src String identifier for mod this data comes from
         * @param ui Finalization status display.
         * @throws std::exception on all kind of errors.
         */
        void load_all_from_json( JsonIn &jsin, const std::string &src, loading_ui &ui,
                                 const std::string &base_path, const std::string &full_path );
        /**
         * Load a single object from a json object.
         * @param jo The json object to load the C++-object from.
         * @param src String identifier for mod this data comes from
         * @throws std::exception on all kind of errors.
         */
        void load_object( const JsonObject &jo, const std::string &src,
                          const std::string &base_path = std::string(),
                          const std::string &full_path = std::string() );

        DynamicDataLoader();
        ~DynamicDataLoader();
        /**
         * Initializes @ref type_function_map
         */
        void initialize();

    public:
        /**
         * Check the consistency of all the loaded data.
         * May print a debugmsg if something seems wrong.
         * @param ui Finalization status display.
         */
        void check_consistency( loading_ui &ui );

        /**
         * Returns the single instance of this class.
         */
        static DynamicDataLoader &get_instance();
        /**
         * Load all data from json files located in
         * the path (recursive).
         * @param path Either a folder (recursively load all
         * files with the extension .json), or a file (load only
         * that file, don't check extension).
         * @param src String identifier for mod this data comes from
         * @param ui Finalization status display.
         * @throws std::exception on all kind of errors.
         */
        /*@{*/
        void load_data_from_path( const std::string &path, const std::string &src, loading_ui &ui );
        /*@}*/
        /**
         * Deletes and unloads all the data previously loaded with
         * @ref load_data_from_path
         */
        void unload_data();
        /**
         * Called to finalize the loaded data. This should be called
         * after all the mods have been loaded.
         * It must be called once after loading all data.
         * It also checks the consistency of the loaded data with
         * @ref check_consistency
         * @param ui Finalization status display.
         * @throw std::exception if the loaded data is not valid. The
         * game should *not* proceed in that case.
         */
        void finalize_loaded_data( loading_ui &ui );

        /**
         * Loads and then removes entries from @param data
         */
        void load_deferred( deferred_json &data );

        /**
         * Returns whether the data is finalized and ready to be utilized.
         */
        bool is_data_finalized() const {
            return finalized;
        }

        /**
         * Get a possibly cached stream for deferred data loading. If the cached
         * stream is still in use by outside code, this returns a new stream to
         * avoid conflict of stream cursor. The stream cursor is not reset if a
         * cached stream is returned.
         */
        shared_ptr_fast<std::istream> get_cached_stream( const std::string &path );
};

namespace init
{

/// Load (or reload) mods' main Lua scripts.
/// @returns the number of loaded scripts.
auto load_main_lua_scripts( cata::lua_state &state, const std::vector<mod_id> &packs ) -> int;

/** Returns whether the game data is currently loaded. */
bool is_data_loaded();

/**
 * Load & finalize modlist that consists of single vanilla BN core "mod".
 * @throw std::exception if the loaded data is not valid.
 */
void load_core_bn_modfiles();

/**
 * Load & finalize modlist needed for the current world.
 * @param ui structure for load progress display
 * @param artifact_file file with per-world artifact definitions
 * @throw std::exception if the loaded data is not valid.
 */
void load_world_modfiles( loading_ui &ui, const world *world, const std::string &artifacts_file );

/**
 * Load soundpack.
 * @param soundpack_path path to soundpack directory.
 * @throw std::exception if the loaded data is not valid.
 */
void load_soundpack_files( const std::string &soundpack_path );

/**
 * Check mods for errors.
 *
 * Does so by individually loading & finalizing each mod with all its dependencies.
 * @param ui structure for load progress display
 * @param opts check specific mods (or all if empty)
 * @return whether all mods were successfully loaded and had no errors
 */
bool check_mods_for_errors( loading_ui &ui, const std::vector<mod_id> &opts );

} // namespace init


