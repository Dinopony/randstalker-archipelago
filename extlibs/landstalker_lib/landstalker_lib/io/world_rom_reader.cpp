#include "io.hpp"

#include "../model/entity_type.hpp"
#include "../model/map.hpp"
#include "../model/world.hpp"
#include "../model/entity.hpp"
#include "../model/blockset.hpp"

#include "../constants/offsets.hpp"
#include "../constants/entity_type_codes.hpp"
#include "../constants/map_codes.hpp"

#include "../assets/entity_type_names.json.hxx"

#include "../tools/vectools.hpp"
#include "../tools/huffman_tree.hpp"
#include "../tools/lz77.hpp"

static void read_map_palettes(const md::ROM& rom, World& world)
{
    uint32_t addr = offsets::MAP_PALETTES_TABLE;

    while(addr < offsets::MAP_PALETTES_TABLE_END)
    {
        MapPalette palette_data {};
        for(uint8_t i=0 ; i<13 ; ++i)
        {
            palette_data[i] = Color::from_bgr_word(rom.get_word(addr));
            addr += 0x2;
        }

        world.add_map_palette(new MapPalette(palette_data));
    }
}

static void read_maps_data(const md::ROM& rom, World& world)
{
    std::map<uint32_t, MapLayout*> map_layout_addresses;

    constexpr uint16_t MAP_COUNT = 816;
    for(uint16_t map_id = 0 ; map_id < MAP_COUNT ; ++map_id)
    {
        Map* map = new Map(map_id);

        uint32_t addr = offsets::MAP_DATA_TABLE + (map_id * 8);

        uint32_t map_layout_addr = rom.get_long(addr);
        if(!map_layout_addresses.count(map_layout_addr))
        {
            MapLayout* layout = io::decode_map_layout(rom, map_layout_addr);
            world.add_map_layout(layout);
            map_layout_addresses[map_layout_addr] = layout;
        }
        map->layout(map_layout_addresses.at(map_layout_addr));

        uint8_t primary_blockset_id = rom.get_byte(addr+4) & 0x3F;
        map->unknown_param_1((rom.get_byte(addr+4) >> 6));

        uint8_t palette_id = rom.get_byte(addr+5) & 0x3F;
        map->palette(world.map_palette(palette_id));
        map->unknown_param_2((rom.get_byte(addr+5) >> 6));

        map->room_height(rom.get_byte(addr+6));

        map->background_music(rom.get_byte(addr+7) & 0x1F);
        uint8_t secondary_blockset_id = (rom.get_byte(addr+7) >> 5) & 0x07;

        // Read base chest ID from its dedicated table
        map->base_chest_id(rom.get_byte(offsets::MAP_BASE_CHEST_ID_TABLE + map_id));

        // Read visited flag from its dedicated table
        uint16_t flag_description = rom.get_word(offsets::MAP_VISITED_FLAG_TABLE + (map_id * 2));
        uint16_t byte = (flag_description >> 3);
        uint8_t bit = flag_description & 0x7;
        map->visited_flag(Flag(byte, bit));

        map->blockset(world.blockset(primary_blockset_id, secondary_blockset_id+1));

        world.add_map(map);
    }
}

static void read_maps_fall_destination(const md::ROM& rom, World& world)
{
    for(uint32_t addr = offsets::MAP_FALL_DESTINATION_TABLE ; rom.get_word(addr) != 0xFFFF ; addr += 0x4)
    {
        Map* map = world.map(rom.get_word(addr));
        map->fall_destination(rom.get_word(addr+2));
    }
}

static void read_maps_climb_destination(const md::ROM& rom, World& world)
{
    for(uint32_t addr = offsets::MAP_CLIMB_DESTINATION_TABLE ; rom.get_word(addr) != 0xFFFF ; addr += 0x4)
    {
        Map* map = world.map(rom.get_word(addr));
        map->climb_destination(rom.get_word(addr+2));
    }
}

static void read_maps_entities(const md::ROM& rom, World& world)
{
    for(auto& [map_id, map] : world.maps())
    {
        uint16_t offset = rom.get_word(offsets::MAP_ENTITIES_OFFSETS_TABLE + (map_id*2));
        if(offset > 0)
        {
            // Maps with offset 0000 have no entities
            for(uint32_t addr = offsets::MAP_ENTITIES_TABLE + offset-1 ; rom.get_word(addr) != 0xFFFF ; addr += 0x8)
                map->add_entity(Entity::from_rom(rom, addr, map));
        }
    }
}

static void read_maps_variants(const md::ROM& rom, World& world)
{
    for(uint32_t addr = offsets::MAP_VARIANTS_TABLE ; rom.get_word(addr) != 0xFFFF ; addr += 0x6)
    {
        Map* map = world.map(rom.get_word(addr));
        Map* variant_map = world.map(rom.get_word(addr+2));

        uint8_t flag_byte = rom.get_byte(addr+4);
        uint8_t flag_bit = rom.get_byte(addr+5);

        map->add_variant(variant_map, flag_byte, flag_bit);
    }
}

static void read_maps_entity_masks(const md::ROM& rom, World& world)
{
    for(uint32_t addr = offsets::MAP_ENTITY_MASKS_TABLE ; rom.get_word(addr) != 0xFFFF ; addr += 0x4)
    {
        Map* map = world.map(rom.get_word(addr));

        uint16_t word = rom.get_word(addr+2);

        uint8_t msb = word >> 8;
        uint8_t lsb = word & 0x00FF;

        bool visibility_if_flag_set = msb >> 7;
        uint8_t flag_byte = msb & 0x7F;
        uint8_t flag_bit = lsb >> 5;
        uint8_t entity_id = lsb & 0x0F;

        map->entities().at(entity_id)->mask_flags().emplace_back(EntityMaskFlag(visibility_if_flag_set, flag_byte, flag_bit));
    }
}

static void read_maps_global_entity_masks(const md::ROM& rom, World& world)
{
    for(uint32_t addr = offsets::MAP_CLEAR_FLAGS_TABLE ; rom.get_word(addr) != 0xFFFF ; addr += 0x4)
    {
        Map* map = world.map(rom.get_word(addr));

        uint8_t flag_byte = rom.get_byte(addr+2);

        uint8_t lsb = rom.get_byte(addr+3);
        uint8_t flag_bit = lsb >> 5;
        uint8_t first_entity_id = lsb & 0x1F;

        map->global_entity_mask_flags().emplace_back(GlobalEntityMaskFlag(flag_byte, flag_bit, first_entity_id));
    }
}

static void read_maps_key_door_masks(const md::ROM& rom, World& world)
{
    for(uint32_t addr = offsets::MAP_CLEAR_KEY_DOOR_FLAGS_TABLE ; rom.get_word(addr) != 0xFFFF ; addr += 0x4)
    {
        Map* map = world.map(rom.get_word(addr));

        uint8_t flag_byte = rom.get_byte(addr+2);

        uint8_t lsb = rom.get_byte(addr+3);
        uint8_t flag_bit = lsb >> 5;
        uint8_t first_entity_id = lsb & 0x1F;

        map->key_door_mask_flags().emplace_back(GlobalEntityMaskFlag(flag_byte, flag_bit, first_entity_id));
    }
}

static void read_maps_dialogue_table(const md::ROM& rom, World& world)
{
    uint32_t addr = offsets::DIALOGUE_TABLE;

    uint16_t header_word = rom.get_word(addr);
    while(header_word != 0xFFFF)
    {
        uint16_t map_id = header_word & 0x7FF;
        uint8_t word_count = header_word >> 11;

        Map* map = world.map(map_id);
        std::vector<uint16_t>& map_speakers = map->speaker_ids();

        for(uint8_t i=0 ; i<word_count ; ++i)
        {
            uint32_t offset = (i+1)*2;
            uint16_t word = rom.get_word(addr + offset);

            uint16_t speaker_id = word & 0x7FF;
            uint8_t consecutive_speakers = word >> 11;
            for(uint8_t j=0 ; j<consecutive_speakers ; ++j)
                map_speakers.emplace_back(speaker_id++);
        }

        addr += (word_count + 1) * 2;
        header_word = rom.get_word(addr);
    }
}

static void read_maps_persistence_flags(const md::ROM& rom, World& world)
{
    uint32_t addr = offsets::PERSISTENCE_FLAGS_TABLE;

    // Read switches persistence flags table
    while(rom.get_word(addr) != 0xFFFF)
    {
        uint16_t map_id = rom.get_word(addr);
        uint8_t flag_byte = rom.get_byte(addr+2);

        uint8_t byte_4 = rom.get_byte(addr+3);
        uint8_t flag_bit = byte_4 >> 5;
        uint8_t entity_id = byte_4 & 0x1F;

        world.map(map_id)->entity(entity_id)->persistence_flag(Flag(flag_byte, flag_bit));
        addr += 0x4;
    }

    addr += 0x2;

    // Read sacred trees persistence flags table by building first a table of sacred trees per map
    std::map<uint16_t, std::vector<Entity*>> sacred_trees_per_map;
    for (auto& [map_id, map] : world.maps())
    {
        for (Entity* entity : map->entities())
        {
            if (entity->entity_type_id() == ENTITY_SACRED_TREE)
                sacred_trees_per_map[map_id].emplace_back(entity);
        }
    }

    std::map<uint16_t, uint8_t> current_id_per_map;
    while(rom.get_word(addr) != 0xFFFF)
    {
        uint16_t map_id = rom.get_word(addr);
        uint8_t flag_byte = rom.get_byte(addr+2);
        uint8_t flag_bit = rom.get_byte(addr+3);
        addr += 0x4;

        if (!current_id_per_map.count(map_id))
            current_id_per_map[map_id] = 0;
        uint8_t sacred_tree_id = current_id_per_map[map_id]++;

        try
        {
            Entity* sacred_tree = sacred_trees_per_map.at(map_id).at(sacred_tree_id);
            sacred_tree->persistence_flag(Flag(flag_byte, flag_bit));
        }
        catch (std::out_of_range&)
        {
            throw LandstalkerException("Sacred tree persistence flag points on tree #"
                                       + std::to_string(sacred_tree_id) + " of map #"
                                       + std::to_string(map_id) + " which does not exist.");
        }
    }
}

static void read_map_connections(const md::ROM& rom, World& world)
{
    for(uint32_t addr = offsets::MAP_CONNECTIONS_TABLE ; rom.get_word(addr) != 0xFFFF ; addr += 0x8)
    {
        MapConnection connection;

        connection.map_id_1(rom.get_word(addr) & 0x3FF);
        connection.extra_byte_1((rom.get_byte(addr) & 0xFC) >> 2);
        connection.pos_x_1(rom.get_byte(addr+2));
        connection.pos_y_1(rom.get_byte(addr+3));

        connection.map_id_2(rom.get_word(addr+4) & 0x3FF);
        connection.extra_byte_2((rom.get_byte(addr+4) & 0xFC) >> 2);
        connection.pos_x_2(rom.get_byte(addr+6));
        connection.pos_y_2(rom.get_byte(addr+7));

        world.map_connections().emplace_back(connection);
    }
}

static void read_custom_map_setups(const md::ROM& rom, World& world)
{
    uint32_t addr = 0x19B62; // DoCustomRoomActionsStart

    while(addr < 0x1A306)
    {
        uint16_t first_opcode = rom.get_word(addr);
        uint32_t map_setup_addr = addr + 10;

        uint16_t offset = rom.get_byte(addr + 9);
        if(offset == 0)
        {
            offset = rom.get_word(addr + 10);
            map_setup_addr += 2;
        }
        uint32_t next_addr = addr + 10 + offset;

        if(first_opcode == 0x0C79) // Regular map ID test (cmpi.w)
        {
            uint16_t map_id = rom.get_word(addr + 2);
            uint32_t tested_addr = rom.get_long(addr + 4);
            if(tested_addr == 0xFF1206)
            {
                // Specific variant: only apply to variant
                world.map(map_id)->map_setup_addr(map_setup_addr);
            }
            else if(tested_addr == 0xFF1204)
            {
                // Parent map: apply to parent & variants
                Map* parent_map = world.map(map_id);
                std::set<Map*> maps_to_process = parent_map->all_recursive_variants();
                maps_to_process.insert(parent_map);

                for(Map* map : maps_to_process)
                    map->map_setup_addr(map_setup_addr);
            }
            else
            {
                // Shouldn't happen!
                throw std::exception();
            }
        }
        else if(first_opcode == 0x0C39) // MusicTreeWarp test (cmpi.b)
        {
            // All Tibor maps
            std::set<Map*> maps_to_process = {
                world.map(808), world.map(809), world.map(810), world.map(811),
                world.map(812), world.map(813), world.map(814), world.map(815)
            };
            map_setup_addr = 0x1A06C;
            for(Map* map : maps_to_process)
                map->map_setup_addr(map_setup_addr);
        }
        else
        {
            // Shouldn't happen!
            throw std::exception();
        }

        addr = next_addr;
    }
}

static void read_custom_map_updates(const md::ROM& rom, World& world)
{
    // Use "CheckAndDoLavaPaletteFx" in magma room
    world.map(MAP_LAKE_SHRINE_EXTERIOR_WITH_MAGMA)->map_update_addr(0x25D0);

    // Use "CheckAndDisplayIntroString" only in intro maps
    world.map(MAP_INTRO_139)->map_update_addr(0xC46A);
    world.map(MAP_INTRO_140)->map_update_addr(0xC46A);
    world.map(MAP_INTRO_141)->map_update_addr(0xC46A);
    world.map(MAP_INTRO_142)->map_update_addr(0xC46A);
    world.map(MAP_INTRO_143)->map_update_addr(0xC46A);
}

static void read_maps(const md::ROM& rom, World& world)
{
    read_map_palettes(rom, world);
    read_maps_data(rom, world);
    read_maps_fall_destination(rom, world);
    read_maps_climb_destination(rom, world);
    read_maps_entities(rom, world);
    read_maps_variants(rom, world);
    read_maps_entity_masks(rom, world);
    read_maps_global_entity_masks(rom, world);
    read_maps_key_door_masks(rom, world);
    read_maps_dialogue_table(rom, world);
    read_maps_persistence_flags(rom, world);
    read_map_connections(rom, world);
    read_custom_map_setups(rom, world);
    read_custom_map_updates(rom, world);
}

///////////////////////////////////////////////////////////////////////////////

static void read_game_strings(const md::ROM& rom, World& world)
{
    uint32_t huffman_trees_base_addr = offsets::HUFFMAN_TREE_OFFSETS + (SYMBOL_COUNT * 2);

    std::vector<HuffmanTree*> huffman_trees;

    std::vector<uint16_t> trees_offsets = rom.get_words(offsets::HUFFMAN_TREE_OFFSETS, huffman_trees_base_addr);
    for(uint16_t tree_offset : trees_offsets)
    {
        if (tree_offset == 0xFFFF)
            huffman_trees.emplace_back(nullptr);
        else
            huffman_trees.emplace_back(io::decode_huffman_tree(rom, huffman_trees_base_addr + tree_offset));
    }

    uint32_t textbank_table_addr = rom.get_long(offsets::TEXTBANKS_TABLE_POINTER);
    std::vector<uint32_t> textbank_addrs;
    for(uint32_t addr = textbank_table_addr ; rom.get_long(addr) != 0xFFFFFFFF ; addr += 0x4)
        textbank_addrs.push_back(rom.get_long(addr));

    world.game_strings() = io::decode_textbanks(rom, textbank_addrs, huffman_trees);
    for(HuffmanTree* tree : huffman_trees)
        delete tree;
}

static void read_entity_type_palettes(const md::ROM& rom, World& world)
{
    // Read entity palettes
    for(uint32_t addr = offsets::ENTITY_PALETTES_TABLE ; rom.get_word(addr) != 0xFFFF ; addr += 0x2)
    {
        uint8_t entity_id = rom.get_byte(addr);
        uint8_t palette_id = rom.get_byte(addr+1);

        if(!world.entity_types().count(entity_id))
            world.add_entity_type(new EntityType(entity_id));

        EntityType* entity_type = world.entity_type(entity_id);
        if(palette_id & 0x80)
        {
            // High palette
            palette_id &= 0x7F;
            uint32_t palette_base_addr = offsets::ENTITY_PALETTES_TABLE_HIGH + (palette_id * 7 * 2);
            EntityHighPalette high_palette_colors = EntityHighPalette::from_rom(rom, palette_base_addr);
            entity_type->high_palette(high_palette_colors);
        }
        else
        {
            // Low palette
            uint32_t palette_base_addr = offsets::ENTITY_PALETTES_TABLE_LOW + (palette_id * 6 * 2);
            EntityLowPalette low_palette_colors = EntityLowPalette::from_rom(rom, palette_base_addr);
            entity_type->low_palette(low_palette_colors);
        }
    }
}

static void load_entity_type_names_from_json(World& world)
{
    // Apply the randomizer model changes to the model loaded from ROM
    Json entity_type_names_json = Json::parse(ENTITY_TYPE_NAMES_JSON);
    for(size_t i=0 ; i<entity_type_names_json.size() ; ++i)
    {
        std::string type_name = entity_type_names_json.at(i);
        world.entity_type(i)->name(type_name);
    }
}

static void read_entity_types(const md::ROM& rom, World& world)
{
    // Init ground item pseudo entity types
    for(auto& [item_id, item] : world.items())
    {
        if(item_id > 0x3F)
            continue;
        uint8_t entity_id = item_id + 0xC0;
        world.add_entity_type(new EntityItemOnGround(entity_id, item));
    }

    // Read item drop probabilities from a table in the ROM
    std::vector<uint16_t> probability_table;
    for(uint32_t addr = offsets::PROBABILITY_TABLE ; addr < offsets::PROBABILITY_TABLE_END ; addr += 0x2)
        probability_table.emplace_back(rom.get_word(addr));

    // Read enemy info from a table in the ROM
    for(uint32_t addr = offsets::ENEMY_STATS_TABLE ; rom.get_word(addr) != 0xFFFF ; addr += 0x6)
    {
        uint8_t id = rom.get_byte(addr);
        std::string name = "enemy_" + std::to_string(id);

        uint8_t health = rom.get_byte(addr+1);
        uint8_t defence = rom.get_byte(addr+2);
        uint8_t dropped_golds = rom.get_byte(addr+3);
        uint8_t attack = rom.get_byte(addr+4) & 0x7F;
        Item* dropped_item = world.item(rom.get_byte(addr+5) & 0x3F);

        // Use previously built probability table to know the real drop chances
        uint8_t probability_id = ((rom.get_byte(addr+4) & 0x80) >> 5) | (rom.get_byte(addr+5) >> 6);
        uint16_t drop_probability = probability_table.at(probability_id);

        world.add_entity_type(new EnemyType(id, name,
                                            health, attack, defence,
                                            dropped_golds, dropped_item, drop_probability));
    }

    read_entity_type_palettes(rom, world);

    // Add the invisible cube entity type which has no palette, so isn't listed anywhere
    world.add_entity_type(new EntityType(0x51));

    load_entity_type_names_from_json(world);
}

static void read_blocksets(const md::ROM& rom, World& world)
{
    // A blockset group is a table of blocksets where the first one is a "primary blockset", and other ones are "secondary".
    // A map always uses the primary blockset concatenated with one of the secondary blocksets in the group.
    // We have a "blockset groups table" which associates blockset group IDs to an actual blockset group.
    // When the same blockset group is encountered several times, only the last occurence is valid.

    std::vector<uint32_t> blockset_groups_addrs;
    for(uint32_t addr=offsets::BLOCKSETS_GROUPS_TABLE ; addr < offsets::BLOCKSETS_GROUPS_TABLE_END ; addr += 0x4)
        blockset_groups_addrs.push_back(rom.get_long(addr));

    std::map<uint32_t, std::vector<uint32_t>> blocksets_in_groups;
    for(uint32_t blockset_group_addr : blockset_groups_addrs)
    {
        if(blocksets_in_groups.count(blockset_group_addr))
            continue;

        uint32_t addr = blockset_group_addr;
        do
        {
            blocksets_in_groups[blockset_group_addr].push_back(rom.get_long(addr));
            addr += 0x4;
        } while(!vectools::contains(blockset_groups_addrs, addr) && addr < offsets::FIRST_BLOCKSET);
    }

    std::map<uint32_t, std::vector<Blockset*>> blockset_groups_by_addr;
    for(const auto& [group_addr, blocksets_in_group] : blocksets_in_groups)
    {
        for(uint32_t blockset_addr : blocksets_in_group)
            blockset_groups_by_addr[group_addr].push_back(io::decode_blockset(rom, blockset_addr));
    }

    std::vector<std::vector<Blockset*>>& blockset_groups = world.blockset_groups();
    for(uint32_t group_addr : blockset_groups_addrs)
        blockset_groups.push_back(blockset_groups_by_addr.at(group_addr));

    // Only keep the last occurence for each blockset group
    for(size_t i=0 ; i<blockset_groups.size() ; ++i)
        if(world.blockset_id(blockset_groups[i][1]).first != i)
            blockset_groups[i].clear();
}

static void read_item_names(const md::ROM& rom, World& world)
{
    uint8_t item_id = 0;
    for(uint32_t addr = offsets::ITEM_NAMES_TABLE ; addr < offsets::ITEM_NAMES_TABLE_END ; )
    {
        uint8_t length = rom.get_byte(addr);
        addr += 1;
        if(length == 0xFF)
            break;

        std::vector<uint8_t> string_bytes = rom.get_bytes(addr, addr + length);
        world.item(item_id)->name(Symbols::parse_from_bytes(string_bytes));

        addr += length;
        ++item_id;
    }
}

static void read_item_pre_uses(const md::ROM& rom, World& world)
{
    for(uint32_t addr = offsets::ITEM_PRE_USE_TABLE ; addr < offsets::ITEM_PRE_USE_TABLE_END ; addr += 0x6)
    {
        uint8_t item_id = rom.get_byte(addr + 0x4);
        if(item_id == 0xFF)
            break;

        uint16_t branch_offset = rom.get_word(addr + 0x02);
        world.item(item_id)->pre_use_address(addr + 0x02 + branch_offset);
    }
}

static void read_item_post_uses(const md::ROM& rom, World& world)
{
    for(uint32_t addr = offsets::ITEM_POST_USE_TABLE ; addr < offsets::ITEM_POST_USE_TABLE_END ; addr += 0x6)
    {
        uint8_t item_id = rom.get_byte(addr + 0x4) & 0x7F;
        if(item_id == 0x7F)
            break;

        uint16_t branch_offset = rom.get_word(addr + 0x02);
        world.item(item_id)->post_use_address(addr + 0x02 + branch_offset);
    }
}

static void read_items(const md::ROM& rom, World& world)
{
    std::map<uint8_t, Item*>& items = world.items();
    for(uint8_t id=0 ; id<0x40 ; ++id)
    {
        uint32_t item_base_addr = offsets::ITEM_DATA_TABLE + id * 0x04;

        Item* item = new Item();

        item->id(id);
        item->max_quantity(rom.get_byte(item_base_addr) & 0x0F);
        item->verb_on_use((rom.get_byte(item_base_addr) & 0xF0) >> 4);
        item->gold_value(rom.get_word(item_base_addr + 0x2));

        world.add_item(item);
    }

    read_item_names(rom, world);
    read_item_pre_uses(rom, world);
    read_item_post_uses(rom, world);
}

static void read_chest_contents(const md::ROM& rom, World& world)
{
    world.chest_contents().reserve(0x100);

    for(uint32_t addr = offsets::CHEST_CONTENTS_TABLE ; addr < offsets::CHEST_CONTENTS_TABLE_END ; ++addr)
    {
        uint8_t item_id = rom.get_byte(addr);
        world.chest_contents().emplace_back(world.item(item_id));
    }
}

///////////////////////////////////////////////////////////////////////////////

void io::read_world_from_rom(const md::ROM& rom, World& world)
{
    read_items(rom, world);
    read_chest_contents(rom, world);
    read_game_strings(rom, world);
    read_blocksets(rom, world);
    read_entity_types(rom, world);
    read_maps(rom, world);
}