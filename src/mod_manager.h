#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "pimpl.h"
#include "ret_val.h"
#include "type_id.h"

struct WORLDINFO;
class JsonObject;
class dependency_tree;
class mod_manager;

const std::vector<std::pair<std::string, std::string> > &get_mod_list_categories();
const std::vector<std::pair<std::string, std::string> > &get_mod_list_tabs();
const std::map<std::string, std::string> &get_mod_list_cat_tab();

struct translatable_mod_info {
    private:
        std::string mod_path;
        std::string name_raw;
        std::string name_tr;
        std::string description_raw;
        std::string description_tr;
        int language_version = 0;
        void update();
    public:
        translatable_mod_info();
        translatable_mod_info( std::string name, std::string description, std::string path );
        std::string name();
        std::string description();
};

struct MOD_INFORMATION {
    private:
        mutable translatable_mod_info translatable_info;

    public:
        std::string name() const;
        std::string description() const;

        void set_translatable_info( translatable_mod_info &&tmi ) {
            translatable_info = std::move( tmi );
        }

        mod_id ident;

        /** Directory to load JSON from relative to directory containing modinfo.json */
        std::string path;

        /** Full path to modinfo.json, for debug purposes */
        std::string path_full;

        /** All authors who have added content to the mod (excluding maintenance changes) */
        std::set<std::string> authors;

        /**
         *  Assigned maintainers responsible for maintaining compatibility with core
         *  @note these should be verbatim GH account names
         */
        std::set<std::string> maintainers;

        /**
         * Arbitrary string that should help maintainers in figuring out
         * what version of the mod the error in a bugreport comes from.
         * Recommended use is to set latest mod update date here.
         */
        std::string version;

        /** If mod uses Lua API, specifies version of the API being used. */
        std::optional<int> lua_api_version;

        /** What other mods must be loaded prior to this one? */
        std::vector<mod_id> dependencies;

        /** What mods cannot be loaded together with this one? */
        std::vector<mod_id> conflicts;

        /** Core mods are loaded before any other mods */
        bool core = false;

        /** Obsolete mods are loaded for legacy saves but cannot be used when starting new worlds */
        bool obsolete = false;

        std::pair<int, std::string> category = { -1, "" };
};

namespace mod_management
{
using t_mod_list = std::vector<mod_id>;

/**
 * Load all modinfo.json files (recursively) from the given root.
 * @param path The root folder from which the modinfo files are searched.
 */
std::vector<MOD_INFORMATION> load_mods_from( const std::string &path );

/**
 * Load all mod information from a json file.
 * (@see load_modfile)
 */
void load_mod_info( const std::string &info_file_path, std::vector<MOD_INFORMATION> &out );

/**
 * Load mod info from a json object.
 * @throws JsonError on all kind of errors.
 */
std::optional<MOD_INFORMATION> load_modfile( const JsonObject &jo, const std::string &path );

/**
 * Save mod list to file.
 * @returns true on success.
 */
bool save_mod_list( const t_mod_list &list, const std::string &path );

/**
 * Load mod list from file.
 * @returns std::nullopt on error.
 */
std::optional<t_mod_list> load_mod_list( const std::string &path );

/**
 * Get id of default core content pack.
 */
mod_id get_default_core_content_pack();

} // namespace mod_management

class mod_manager
{
    public:
        using t_mod_list = mod_management::t_mod_list;

        mod_manager();
        ~mod_manager();
        /**
         * Reload the map of available mods (@ref mod_map).
         * This also reloads the dependency tree.
         */
        void refresh_mod_list();

        std::vector<mod_id> all_mods() const;

        /**
         * Returns the dependency tree for the loaded mods.
         * @returns @ref dependency_tree
         */
        dependency_tree &get_tree();
        /**
         * Clear @ref mod_map and delete @ref dependency_tree.
         */
        void clear();

        /**
         * Save list of mods that are active in that world to
         * the world folder.
         */
        void save_mods_list( WORLDINFO *world ) const;
        /**
         * Load list of mods that should be active in that
         * world.
         */
        void load_mods_list( WORLDINFO *world ) const;
        const t_mod_list &get_default_mods() const;
        bool set_default_mods( const t_mod_list &mods );
        std::vector<mod_id> get_all_sorted() const;

    private:
        // Make this accessible for now
        friend class mod_ui;
        friend class worldfactory;
        friend mod_id;
        /**
         * @returns path of a file in the world folder that contains
         * the list of mods that should be loaded for this world.
         */
        static std::string get_mods_list_file( WORLDINFO *world );

        /**
         * Add mods from given list to the pool.
         * Mods with same id are overwritten.
         */
        void add_mods( std::vector<MOD_INFORMATION> &&list );

        void remove_mod( const mod_id &ident );
        void remove_invalid_mods( t_mod_list &mods ) const;
        void load_replacement_mods( const std::string &path );

        pimpl<dependency_tree> tree;

        /**
         * The map of known mods, key is the mod ident.
         */
        std::map<mod_id, MOD_INFORMATION> mod_map;
        t_mod_list default_mods;
        /** Second field is optional replacement mod */
        std::map<mod_id, mod_id> mod_replacements;
};

class mod_ui
{
    public:
        mod_ui( mod_manager &mman );

        std::string get_information( const MOD_INFORMATION *mod );
        mod_manager &active_manager;
        dependency_tree &mm_tree;

        ret_val<bool> try_add( const mod_id &mod_to_add, std::vector<mod_id> &active_list );
        void try_rem( size_t selection, std::vector<mod_id> &active_list );
        void try_shift( char direction, size_t &selection, std::vector<mod_id> &active_list );

        bool can_shift_up( size_t selection, const std::vector<mod_id> &active_list );
        bool can_shift_down( size_t selection, const std::vector<mod_id> &active_list );
};


