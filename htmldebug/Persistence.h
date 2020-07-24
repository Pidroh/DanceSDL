#pragma once
#include <stdio.h>
#ifdef __EMSCRIPTEN__
#include "emscripten.h"
#endif

namespace persistence
{

	SDL_RWops* file;
	std::vector<int> saveData;
	int loadIndex = 0;

	void Init(const char* dirName)
	{
#ifdef __EMSCRIPTEN__
		EM_ASM({
		var dirN = UTF8ToString($0);
		// Make a directory other than '/'
		FS.mkdir(dirN);
		// Then mount with IDBFS type
		FS.mount(IDBFS, {}, dirN);
		//alert("ssSYNCCALLED");
		console.log("ss");
		// Then sync
		FS.syncfs(true, function(err) {

			//alert(dirN);
			//alert("folder SYNCCALLED");
			//alert(err);
			ccall('mainA', 'v');

			// Error
		});
			}, dirName);
#endif
	}

	void LoadStartFromString(std::string datasave) 
	{
		saveData.clear();
		loadIndex = 0;
		auto nstrings = split(datasave, ',');
		for (size_t i = 0; i < nstrings.size(); i++)
		{
			saveData.push_back(std::stoi(nstrings[i]));
		}
	}

	std::string SaveDataAsString() 
	{
		std::string nstrings = "";
		for (size_t i = 0; i < saveData.size(); i++)
		{
			nstrings += std::to_string(saveData[i]);
			nstrings += ',';
		}
		return nstrings;
	}

	void LoadStart(const char* filename)
	{
		saveData.clear();
		loadIndex = 0;
		for (size_t i = 0; i < 3; i++)
		{
			file = SDL_RWFromFile(filename, "r");
			
			if (file != NULL) {
				while (true) 
				{
					int a = 0;
					int r = SDL_RWread(file, &a, sizeof(Sint32), 1);
					saveData.push_back(a);
					if (r == 0) 
					{
						break;
					} 
				}
				
				break;
			}
		}
		
		if (file == NULL) //create file if null
		{
			saveData.push_back(-1);
		}
		else 
		{
			SDL_RWclose(file);


#ifdef __EMSCRIPTEN__
			EM_ASM({
			var dirN = UTF8ToString($0);
			// Make a directory other than '/'
			FS.unlink(dirN);
				}, filename);


#endif
		}
	}

	void LoadEnd(const char* filename)
	{



		//remove(filename);

	}

	void LoadEndAll()
	{
#ifdef __EMSCRIPTEN__
		EM_ASM({

				var db;
				var request = indexedDB.open("/savedata");
				request.onerror = function(event) {
					console.log("Why didn't you allow my web app to use IndexedDB?!");
				};
				request.onsuccess = function(event) {
					db = event.target.result;
					//alert("success");
					//db.deleteObjectStore("/savedata/game.progress.bin");
					//var trans = db.transaction(['FILE_DATA'], "readwrite").objectStore('FILE_DATA').delete("/savedata/gameprogress.bin");
					var trans = db.transaction(['FILE_DATA'], "readwrite").objectStore('FILE_DATA').clear();
					trans.onsuccess = function(event)
					{
						//FS.syncfs(false, function(err) {
							//alert("main A!!");
							//alert("SYNCCALLED COMMIT");
							//alert(err);
							// Error
						//});
						//alert("success2");
					}
				};


			});

#endif
	}

	bool SaveStart(const char* filename)
	{
		saveData.clear();
		file = SDL_RWFromFile(filename, "w");
		if (file == NULL)
		{
			return false;
		}

		//SDL_Log("save start");
		return true;
	}

	void SaveEnd()
	{
		SDL_RWclose(file);
	}

	void WriteInt(int i)
	{
		
		SDL_RWwrite(file, &i, sizeof(Sint32), 1);
		saveData.push_back(i);
	}

	void WriteBool(bool b)
	{
		// SDL_RWwrite(file, &b, sizeof(bool), 1);
		WriteInt((int)b);
		//saveData.push_back((int)b);
	}

	bool DataLeftToLoad() 
	{
		return saveData.size() > loadIndex;
	}

	int ReadInt()
	{
		
		int a = 0;
		a = saveData.at(loadIndex);
		loadIndex++;
		//SDL_RWread(file, &a, sizeof(Sint32), 1);
		return a;
	}
	
	bool ReadBool()
	{
		bool b = false;
		b = (bool) saveData[loadIndex];
		loadIndex++;
		//SDL_RWread(file, &b, sizeof(bool), 1);
		return b;
	}

	void Commit()
	{
#ifdef __EMSCRIPTEN__
		EM_ASM(
			// Then sync
			FS.syncfs(false, function(err) {
			//alert("main A!!");
			//alert("SYNCCALLED COMMIT");
			//alert(err);
			// Error
		});
		);
#endif
	}

	void Reset(const char* filename)
	{
		SaveStart(filename);
		WriteInt(-1);
		SaveEnd();
		Commit();

	}


}
