#ifndef FILES_H
#define FILES_H

#define CHUNK_SIZE_BYTES 1000000

#include "front_end_kv_client.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#include "front_end_kv_client.h"


using namespace std;
using namespace rapidjson;


bool init_files_metadata(string username, KeyValueClient &client);
int write_file(string username, string filename, string data, KeyValueClient &client);
string get_files(string username, KeyValueClient &client);
string get_file_data(string username, string file_name, KeyValueClient &client);
string get_files_in_dir(string username, string directory_path, KeyValueClient &client);
string get_dirs_in_dir(string username, string directory_path, KeyValueClient &client);
pair<string, string> get_all_files_json(string username, string root, Document::AllocatorType& allocator, KeyValueClient &client);
string one_level_up(string path);
void create_dir(string username, string directory_path, KeyValueClient &client);
bool build_out_directory_path(string username, string absolute_file_path, KeyValueClient &client);
bool delete_file(string username, string absolute_file_path, KeyValueClient &client);
bool file_exists(string username, string file_name, KeyValueClient &client);
int move_file(string username, string old_absolute_filepath, string new_absolute_filepath, KeyValueClient &client);
int rename_dir(string username, string old_dir_name, string new_dir_nams, KeyValueClient &client);
string get_shared_files_metadata(string username, KeyValueClient &client);
bool share_file(string sender, string recipient, string absolute_file_path, KeyValueClient &client);

bool delete_dir(string username, string dir_name, KeyValueClient &client);
int count_slashes_in_path(string path);
int rename_dir(string username, string old_dir_name, string new_dir_name, KeyValueClient &client);
string replace_in_path(string path, string to_replace, string replace_with);


#endif /* FILES_H */
