#include <SDL.h>
#include "SDL_thread.h"
#include "SDLCode.h"
#include "Vector2D.h"
#include "Entities.h"
#include <stdlib.h>
#include <array>
#include "Main.h"
#include "Profiler.h"
#include "Sound.h"
#include <vector>
//#include <string>
#include "ParticleExecuter.h"
#include <chrono>
//#include "EnemyFollow.h"
#include "RandomEx.h"
#include "ScreenTransition.h"
#include "BattleLoop.h"
#include "Persistence.h"
#include "Story.h"
#include "FileUtils.h"
#include "AtlasManager.h"
#include "tmxparser.h"
#include "IncrementalSDL.h"
#include "VariableGP.h"
#include "Sound.h"
#include "VectorUtils.h"
#include "BattleChoice.h"
#include "MapRendering.h"
#include "SpiritXPScreen.h"
#include "Dialog.h"
#include "TransformAnimation.h"
#include "Config.h"
#include "Stats.h"
#include "Quest.h"
#include "Farm.h"
#include "BrowserSaveIntegration.h"
#include "DungeonMap.h"
#ifdef __EMSCRIPTEN__
#include "emscripten.h"
#endif


enum MainState {
	ems_Title, ems_Battle, ems_Gameover, ems_MainStory, ems_StoryCharge, ems_Choice, ems_Room,
	ems_SpiritXP, ems_Incremental, ems_None, ems_Cutscene, ems_MapSpot
};

/*
  ゲームのメイン
  ゲームのルールと管理はここで行われます
*/


//does not contain all of it though, should be reset when loading
struct PersistentData {
	std::vector<std::string> cutscenesSeen;
};

PersistentData persData;

bool gameRunning = true;
SDL_Color color_background = { 30,10,30,255 };
SDL_Color color_bright = { 255,236,39,255 };

//SDL_Color color_darkgreen = { 0,135, 81,255 };
SDL_Color color_bright2 = { 255,0,77,255 };

int width = 640;
int height = 480;
std::string finishText = "Gameover";

std::vector<Vector2D> deadenemypositionsForRoom;

bool drawDebug = false;

float deltaMultiplier = 1.0f;
float deltaOverwrite = -1;
float timeOnState = 0;

//ImageRenderDataAcc irda;
MainState state = ems_MainStory;
MainState nextState = ems_MainStory;
MainState nextStateAfterSpiritXP = ems_Gameover;
float battleStartTime = 0;
float battleDuration = 0;

int storyCurrent = 0;
int storyProgress = 0;
int storyProgressM = 0;
float storyGoal = 120.0f;
float storyGoalP = 0.0f;
float storyGoalP_Last = 0;
float timeInStoryCharge = 0;
vector<int> storiesSeen;

std::string roomAlias = "";

vector<vector<const char*>> storyMessages = {
	{
	"A girl walks into a forest",
	"And the darkness of the trees engulf her once more."
	},
	//1
	{
	"--- Chapter Alpha ---",
	"Wind gushed through the girl, engulfed by a spirit",
	"The demons of the forest attempt to surround and defeat her, but...",
	"They become entranced by the girl. All their anger turned to smiles.",
	"...",
	"Cyan Girl\nI got some new info.",
	"Spirit\nSpeak, Sara.", "Sara\nA little boy has been coughing at night",
	"Spirit\nA cough. Is that what concerns humans? A bit pathetic",
	"Sara\nWell, sorry for being pathetic.",
	"Sara\nThe coughing is pretty bad and the kid has trouble breathing.",
	"Sara\nNo demon related to that?",
	"Iracema\n ...",
	"The spirit, Iracema, walked back into the forest, leaving a puzzled Sara behind."
	},
	//2
	{
	"Sara\nWhat are we even looking for?", "Iracema\nReveal yourself, Boimokoi",
	"Fire appears out of thin air. The flames become a serpent that stands before Sara and Iracema",
	"Sara\nWow, that was cool",
	"Boimokoi\nYou have become crazy, Iracema",
	"Sara\nSo the kid is coughing because of the smoke from the fire?",
	"Boimokoi\nIt is not too late, I can talk to lord Caipora.",
	"Boimokoi\nLet us kill that girl together, Iracema. He will forgive you.",
	"Iracema\nForgive, snake, you say.",
	"Iracema\nMay the forest hear that I call Caipora here.",
	"Iracema\nAnd he will bow to our will before the moon sleeps.",
	"Boimokoi\nOh for Tupa's sake. You're so stupid.",
	"Boimokoi\nSuit yourself.",
	"The snake turns to smoke and vanishes",
	"Sara\nGod, you keep having those dramatic conversations and the snake got away.",
	"Iracema\nIt will have time to prepare. This way we can have an honorable confrontation.",
	"Sara\nAn honorable confrontation? Really?"
	},
	//3
	{
		"Boimokoi\nPlease, please stop.",
		"Boimokoi\nIf you continue I will be cursed forever",
		"Sara\nCurse? This is a gift.",
		"Sara\nYou'll be happy forever.",
		"Boimokoi\nWhat do you THINK you know?!",
		"Boimokoi\nWe have been charmed for years, we are finally free now!",
		"Iracema\nTo rule the forest we only have to charm the lords.",
		"Iracema\nHe is harmless now.",
		"Boimokoi\nYes! The kid is safe, I will leave him alone!",
		"Sara\nHmmm...",
		"Iracema\nJust go, snake. Before our minds are changed.",
		"Boimokoi\nThank you! Thank you!!!",
		"Sara\nWhat are you saying, Iracema?",
		"The winds engulf Sara once more.",
		"The demon can't help but let go, with wide open eyes of despair.",
		"They soon turn into a serene, joyful smile. The snake laughs away, beautifully",
		"Iracema\n...",
		"Sara\nHelping out the kid was good, of course. But that wasn't the purpose.",
		"Sara\nYour goal may be to rule the forest, but we are going to make this entire forest, this entire city smile.",
		"Sara walks away, leaving Iracema alone to her thoughts",
		"Iracema\nHumans really are savages, are they not?",
		"Iracema looks up to the sky, as if it could hear her.",
		"Iracema\nEverything is according to plan, huh.",
	},
	//4
	{
		"Sara\nI'm home!",
		"Man's voice\nOkaeri, Sara",
		"Sara's expression hardened as she heard her father speak Japanese.",
		"Little boy\nSara, Sara, help me with my math homework!",
		"Sara\nSure Tommy, just a second.",
		"Sara put her stuff back in her room and went to get a glass of water in the kitchen.",
		"Her eyes closed for a second but soon opened as she realized she was almost sleeping.",
		"Tommy\nYou don't have to help if you're tired...",
		"Sara\nC'mon, I'm not tired. Just let me take care of the laundry first.",
		"Father\nTommy already took care of the laundry!",
		"Sara looked at her little bother, surprised.",
		"Sara\nWow Tommy, thanks!!",
		"Tommy\nHehehe",
		"Sara\nYou should learn from Tommy, dad.",
		"Father\n...",
		"Sara had never seen her father doing housework.",
		"Sara\nC'mon Tommy, show me your homework",
	"After Sara finished teaching her little brother, she unlocked her phone.",
	"BeautyMon34\nMy son's cough got better!! He seems to be feeling good now.",
	"BeautyMon34\nMaybe he won't cough tonight... Fingers crossed!!",
	"Sara smiles while she read the message board and her phone falls from her hand as she falls asleep",
	},
	//5
	{
		"Sara is on her way home when she sees a young man.",
		"Sara\nHey Jorge, what's up?",
		"The man next to Sara did not turn around",
		"Sara\nJorge?",
		"Sara\nAre you there, Jorge? Earth to Jorge!",
		"Jorge\nOh, sorry Sara. Didn't see you there...",
		"Sara\nAre you okay? You seem a little down",
		"Jorge\nNah, I'm fine. Talk to you later, ok?",
		"Sara\nSure...",
		"Jorge walks into his house. Sara also heads home.",
		"Sara\nHey Tommy, do you know what is up with Jorge?",
		"Tommy\nOh, you noticed it too. He won't play with me anymore.",
		"Sara\nHuh... That is weird.",
		"Tommy\nWorried about your boooyfriend?",
		"Sara\nHe is not my... Uh, why do I even bother.",
	},
	//6
{
	"Sara\nIracema, a friend of mine has been in a bad mood lately.",
	"Iracema\nNot every human problem is caused by a demon.",
	"Sara\nWell, yeah, but...",
	"Iracema\nWorrying will lead nowhere.",
	"Voice\nI would worry if I were her",
	"Sara\nWho's there?",
	"Voice\nYou have caused too much grief to the forest.",
	"Iracema\nIt's Caipora.",
	"Sara\nThe lord of the forest?",
	"Caipora's voice\nYou hurt my children, I hurt your friends.",
	"Sara\nI didn't hurt your children, I set them free.",
	"Caipora's voice\nThen I shall set the one named Jorge free aswell.",
	"Sara\nWhat th...?!",
	"Caipora's voice\nIf you want Jorge to be safe, never come to the forest again.",
	"Sara\nCome out here and fight, you coward!!!",
	"Iracema\nCalm down Sara.",
	"Sara\nCalm down?!",
	"Iracema\nIf we keep advancing in the forest, we are bound to find him.",
	"Sara\n...",
	"Sara\nFine...",
},
//7
{
	"Sara\nHey, Jorge!",
	"Jorge\nWhat do you want?",
	"Jorge's face doesn't have a fraction of a smile",
	"Sara\nWhy don't you stop by our place? We haven't talked in a while.",
	"Jorge\n...Maybe some other day.",
	"Sara\nC'mon, don't push us away!",
	"Jorge\nSome other day, Sara.",
	"Sara\nWe're friends, right? Rely on me!",
	"Jorge\n...",
	"Sara\nI don't know what you are going through, but..",
	"Jorge\nJUST LEAVE ME ALONE!",
	"Sara\n...",
	"Jorge\nSorry, I...",
	"Jorge leaves Sara by herself.",
	"Sara\nJorge... Don't you die on me, man.",
},
//8
{
	"Sara\nWhere is Caipora!? We have been looking for days!!",
	"Iracema\nCalm down.",
	"Sara\nMy friend is in danger!",
	"Iracema\nIt takes time. We can only invade his territory after we have more energy.",
	"Sara\nHow to get energy faster?",
	"Iracema\nWe get energy when we affect the human world.",
	"Sara\nLike when we helped the kid coughing?",
	"Iracema\nThat is correct.",
	"Sara\nSo unless I find people in trouble and then we locate the demon...",
	"Iracema\nThere is another way.",
	"Sara\nAnother way?",
	"Iracema\nWe find a demon with many connections to the human world, then we charm them",
	"Sara\nSo we help a lot of people at the same time?",
	"Iracema\nIncorrect. The demons might not doing anything to the humans in the first place.",
	"Sara\nWhat will happen to the humans connected to the demons, then?",
	"Iracema\nSince they are already healthy people, the effect of the charm will be stronger.",
	"Iracema\nThey might completely lose their past life.",
	"Iracema\nMight become forever lost in \"happiness\".",
	"Sara\n...",
	"Sara\nLike a happiness coma, huh.",
	"Sara\nThe people I have helped before change, but they can still live their lives.",
	"Sara\nI would even say they can live better.",
	"Iracema\nYes, but if you do it to someone who doesn't need help, the effect might be worse than with the demons.",
	"Sara\nI see.",
	"Sara\nI need time to think.",
},
//9
{
	"Sara\nThanks a ton, Jorge. That pizza was so good.",
	"Jorge\nYou know it.",
	"Sara\nI wish I could cook as good as you can.",
	"Jorge\nKeep dreaming.",
	"Jorge laughed as he washed the dishes.",
	"Sara\nYou should teach a thing or two to my dad.",
	"Jorge\nHey Sara...",
	"Sara\nWhat up?",
	"Jorge\nRemember in fifth grade when I gave you those flowers on valentine?",
	"Sara\nWhat? That never happened.",
	"Jorge turns around surprised, his hands full of foam.",
	"Sara\nIt was in the fourth grade.",
	"Jorge\nNo, it was on the fifth.",
	"Jorge\nAnyways, why didn't you want to become my girlfriend?",
	"Sara\nBecause you were a bad kisser.",
	"Jorge\n...C'mon. We both know that is not true.",
	"Sara\nYou didn't try hard enough.",
	"Jorge\nHuh?",
	"Sara\nI refused you just once and you gave up.",
	"Jorge\nWhat? That's the reason? Girls are so dumb.",
	"Sara\nPeople are dumb. I was dumb for refusing, you were dumb for giving up.",
	"Someone outside interrupts their conversation",
	"Voice\nI'm home J, Sara!",
	"Sara\nHi Paula!!",
	"Jorge\nWelcome home honey.",
	"Jorge kisses Paula passionetely.",
	"Jorge\nC'mon baby, tell her I'm a good kisser.",
	"Paula\nYou two were kissing since fourth grade, right? She kno-",
	"The conversation died in Sara's ears. Everything turned dark.",
	"She then opened her eyes. Tommy had woken her up from her dream.",
	"Tommy\nSara!! Sara, wake up!!",
	"Sara\nDon't wake me up, Tommy...",
	"Tommy\nJorge has been taken to the hospital!!",
},
//10
{
	"Iracema\nYou sure you're gonna do it?",
	"Demon\nDo what...? L, leave me alone!!",
	"The wind hits and hits. The demon tries to resist, but...",
	"Soon they are smiling together, in harmony.",
	"Iracema\n...",
	"They just keep going and going. A never ending motion.",
	"Iracema\nThat is enough.",
	"Sara\nIs it, really? Do we have enough of this stupid energy now?",
	"Iracema\nI believe so.",
	"Sara\nYou believe so. So reassuring.",
	"Iracema\n...",
	"Sara\nI hope you're right.",
	"Sara enters the woods.",
	"Iracema looks at the demon laughing and dancing.",
	"Iracema\nNow the victims are both human and demon.",
	"Iracema\nHow many more will there be until...",
	"Iracema sighs without completing her phrase and looks up to the sky.",
	"...",
	"Caipora's voice\nWhat have you done?! Have you gone crazy?",
	"Sara\nOh, that's your fault.",
	"Sara\nYou should have stayed away from Jorge.",
	"Caipora's voice\nYou would sacrifice not only our kind but also yours?",
	"Caipora's voice\nFor... Power? And it's my fault?!",
	"Caipora's voice\nYou could have stayed out of the forest!! I gave you a chance!",
	"Sara\nI'm only leaving this forest once every single of you is smiling with me",
	"Caipora's voice\nThen come, human.",
	"Caipora's voice fades as Sara advances into the forest."

},
//11
{
	"Caipora\nDamn you, Iracema!",
	"Caipora\nHow could you betray us...",
	"Iracema\nThe forest betrayed me first.",
	"Sara\nI don't forgive you, Caipora, but still...",
	"Sara\nYou can be happy forever.",
	"Caipora\nI will not beg.",
	"Caipora\nBut you'll regret what you are doing. Trust my words, the day will come.",
	"Sara\nHeh...",
	"Sara\nRegret is a cheap price to pay.",
	"And Caipora smiled through the night.",
	"...",
	"Sara runs off towards the demons in the forest, hoping to leave as fast a possible.",
	"Iracema\n She really did it.",
	"Iracema follows after her.",
},
{//12
	"A short of breath Sara bursts into the doctor's room.",
	"Sara\nHow is he doing, doctor?!",
	"Doctor\nI don't wanna give you any false hopes, but just a couple of minutes ago his conditions got much better.",
	"Doctor\nIf he keeps going like this there are no more risks to his life and he should be getting better soon.",
	"Sara puts her hands to her eyes and smiles, reliefed.",
	"Doctor\nHe still hasn't woken up from the coma though, but that should not take too long.",
	"Sara\nThank God... Thank you, really.",
	"Doctor\nGo home now, you look really tired.",
	"...",
	"Tommy\nWow sis, you look like a mess.",
	"Sara\nJust got back from the hospital.",
	"Sara\nThe doctor said Jorge is getting better!",
	"Tommy\nWow sis, that's awesome.",
	"Sara gives out a big smile to her little brother.",
	//"Sara\nYeah!",
	"Tommy stares at Sara for a couple of seconds.",
	"Tommy\nSara, can you give me a hug?",
	"Sara\n...",
	"The two of them embrace.",
	"Sara's smile disappears as she hides her face on her brother's shoulder.",
	"Sara\nI thought he was gonna die, Tommy.",
	"Tommy\nI was pretty worried too.",
	"Tommy\nBut things are fine now.",
	"Sara\nI never wanted for him to get hurt.",
	"Tommy\nWhat are you saying? It's not your fault...",
	"Sara tightens up the hug as her tears fall down on Tommy's shoulder.",
	"...",
	"Sara\nI'm the big sister here, you know. I'm the one who is supposed to take care of you.",
	"Tommy\nYou took care of me for so many years. I wanna help too! ",
	"Sara\nBig words for a kid who hasn't done his homework yet.",
	"Tommy\nWhat the... How did you know?! ",
	"Sara spends a warm night with her brother.",

},
{//13
	"A message board is being visualized through a smartphone",
	"bunnybunny13\nI am pretty worried about my husband.",
	"bunnybunny13\nHe hasn't stopped dancing for days, he is always laughing, barely talks to me.",
	"dreadlockX\nYou probably won't believe me, but my friend's sister just got sent to a psychiatric hospital.",
	"dreadlockX\nShe had the exact same symptoms!",
	"bunnybunny13\nPlease don't joke about this, I am seriously concerned. I want my husband back!",
	"dreadlockX\nI am serious ma'am. I can pm you my friend's info later, if you want. Maybe you two can help each other.",

	"Iracema speaks up to the girl using her phone.",
	"Iracema\nI suppose they are your victims...",
	"Sara\nDon't look at other people's screen, it's rude.",
	"Sara gets up and cleans the dirt in her clothes.",
	"Iracema\nHave you regrets?",
		"Sara\nHonestly, this whole thing sucks.",
		"Sara\nBut I did what I had to do.",
		"Iracema stares dawn into the ground, unable to find words.",
		"Sara\nLet's get moving though.",
		"Sara\nWe have some dancing to do.",
		"And the two of them walk into the night.",
		"...",
		"--- Chapter 1 ---\n\n\nCompleted.",
		"To be continued.",
		"Thank you very much for playing the first chapter.\n\n Join our Discord Server. You will find the latest alphas and you can help make Brisa the best game it can be.",
		"There is still some extra story left!"
},

{//14
	"Sara\nWhat was that thing?!",
	"Iracema\nWhat 'thing' are you referring to?",
	"Sara\nThat strong Boitata. ",
	"Sara\nYou know, the one who almost kicked our ass",
	"Iracema\nOh, that was a phantom of Boimokoi",
	"Sara\nPhantom? Like a ghost?",
	"Iracema\nNo, it is actually...",
	"Iracema\n...",
	"Sara\nWhat?",
	"Iracema\nNevermind",
	"Sara\nNevermind what?",
	"Iracema\nIt is best to forget of it.",
	"Sara\nC'mon, you gotta tell me.",
	"Iracema\n...",
	"Iracema ignores Sara and leaves her by herself.",
	"Sara\nWhat is a phantom?",

},

//{"I am an ordinary person. Just a dance instructor.", "Until I head to the forest."},
//{"Sara is an ordinary person.\nJust a dance instructor.", "Until she heads into the forest."},
//{"Sara\nYou are angry.", "Iracema\n...", "Sara\nC'mon, it's brain washing.", "Iracema\nYou told me you would make them all dance.", "Argh, how many times are we going to have this discussion?", "Iracema\nYou started this conversation.", "Sara\nI guess I did..."}
};

int latestSaveVersion = 14;
int currentSaveVersion = -1;
bool hasBattleData = false;
std::vector<int> loadedBattleData;

auto saveFilename = "savedata/gameprogress.bin";
auto saveFilenameB = "savedata/gameprogressb.xml";

bool IsCutsceneSeen(std::string cutsceneId)
{
	if (VectorUtils::VectorContains(persData.cutscenesSeen, cutsceneId))
	{
		return true;
	}
	return false;
}

bool LoadDataRaw()
{
	loadedBattleData.clear();
	int saveVersion = persistence::ReadInt();
	currentSaveVersion = saveVersion;

	if (saveVersion < 14)  //does not allow old saves
		return false;

	StoryBrisa::GetExecutionData() = StoryBrisa::CutsceneExecutionData();
	storiesSeen.clear();
	enemyDefeatedAmount_pers.resize(0);
	enemyDefeatedAmount_pers.resize(30);
	storyProgress = 0;
	storyGoalP = 0;
	powerup::joinedSpirits.clear();

	powerup::activeSpirits.clear();
	VariableGP::variables.clear();
	VariableGP::variables.resize(100);
	hasBattleData = false;

	

	
	{
		if (saveVersion >= 0)
		{
			storyProgress = persistence::ReadInt();
			storyGoalP = persistence::ReadInt();
		}
		//SDL_Log("Save version %d storyprog %d", saveVersion, storyProgress);
		if (saveVersion >= 1)
		{
			for (size_t i = 0; i < enemyDefeatedAmount_pers.size(); i++)//30
			{
				enemyDefeatedAmount_pers[i] = persistence::ReadInt();
			}
		}
		if (saveVersion >= 2)
		{
			for (size_t i = 0; i < 100; i++)
			{
				auto story = persistence::ReadInt();
				if (story >= 0)
				{
					storiesSeen.push_back(story);
				}

			}
		}
		if (saveVersion >= 5)
		{

			for (size_t i = 0; i < powerup::maxJoinableSpiritAmountCeiling; i++)
			{

				int enemyId = persistence::ReadInt();
				int level = persistence::ReadInt();
				int xp = persistence::ReadInt();
				int pu1 = persistence::ReadInt();
				int pu2 = persistence::ReadInt();
				int pu3 = persistence::ReadInt();




				//if (enemyId >= 0)  //should always add because order matters
				//different order might screw up active spirit id
				{

					powerup::Spirit sp;
					sp.enemyId = enemyId;
					sp.level = level;
					sp.xp = xp;
					sp.powerUpIds[0] = pu1;
					sp.powerUpIds[1] = pu2;
					sp.powerUpIds[2] = pu3;
					if (saveVersion < 13)
					{
						if (saveVersion >= 8)
						{
							persistence::ReadInt() / 10.0f;
							persistence::ReadInt() / 10.0f;
						}
						if (saveVersion >= 11)
						{
							persistence::ReadInt() / 10.0f;
							persistence::ReadInt() / 10.0f;
							persistence::ReadInt(); //maxlevel legacy
						}
					}
					if (saveVersion >= 13)
					{
						sp.quality = persistence::ReadInt();
						sp.farmlevel = persistence::ReadInt();
						sp.farmxp = persistence::ReadInt();
					}


					powerup::joinedSpirits.push_back(sp);
				}
			}

			for (size_t i = 0; i < powerup::maxActiveSpiritCeiling; i++)
			{
				int act = persistence::ReadInt();
				if (act >= 0)
					powerup::activeSpirits.push_back(act);
			}
			if (saveVersion >= 14)
			{
				for (size_t i = 0; i < powerup::maxFarmSpiritCeiling; i++)
				{
					int act = persistence::ReadInt();
					if (act >= 0)
						farm::spiritsInFarm.push_back(act);
				}
			}

		}
		if (saveVersion >= 7)
		{
			SpiritXPScreen::totalXP = persistence::ReadInt();
		}

		if (saveVersion >= 10)
		{

			for (size_t i = 0; i < 100; i++)
			{
				VariableGP::variables[i] = persistence::ReadInt();
				//VariableGP::variables.push_back(persistence::ReadInt());
			}
		}


		if (saveVersion >= 3)
		{
			bool battleDataSave = persistence::ReadBool();
			hasBattleData = battleDataSave;
			if (battleDataSave)
			{
				std::vector<int> battleDataLoaded;

				int battleDataSize = persistence::ReadInt();

				for (size_t i = 0; i < battleDataSize; i++)
				{
					if (persistence::DataLeftToLoad())
					{
						battleDataLoaded.push_back(persistence::ReadInt());
					}
					else
					{
						SDL_Log("Save data corrupted!");
						break;
					}

				}
				loadedBattleData = battleDataLoaded;
			}
		}

	}


	persistence::LoadEnd(saveFilename);

	powerup::SpiritRemoveRepeats();
	if (powerup::activeSpirits.size() > 3) 
	{
		powerup::activeSpirits.resize(3);
	}
	return true;
}

bool LoadDataFromString(std::string data)
{
	persistence::LoadStartFromString(data);

	return LoadDataRaw();
}

namespace gameover //#gameover
{
	enum state
	{
		filling, complete
	};
	state st;
	float timeInState;

	void Enter()
	{
		st = filling;
		timeInState = 0;
	}

	int Update(float time, bool wantToProgress)
	{
		if (time == 0) time = 0.0000001f;
		timeInState += time;

		float reportedScore = BattlGetScore();
		int battleScore = BattlGetScore();
		state next = st;
		float scale = 2;
		SDL_Color c = { 255,255,255,255 };
		switch (st)
		{
		case gameover::filling:
		{
			//T sec
			//T - 1 
			//0 - 0

			//1 - t*t
			float runTime = 0;
			float speed = reportedScore;
			float initSp = speed;
			float desac = speed * 0.5f;
			reportedScore = 0;
			auto delta = 0.032f;
			float minSpeed = 0.8f;
			if (time != 0)
			{
				while (runTime < timeInState)
				{
					runTime += delta;
					speed -= desac * delta;
					speed = (1.0f - (reportedScore / battleScore))*battleScore*1.2f;
					float speedB = 25 * 25 * 25;
					while (battleScore - reportedScore < speedB)
					{
						speedB = speedB / 10;
						//if (battleScore - reportedScore < 3.1f) 
						//{
						//	speedB = 3.0f / 8.0f;
						//	break;
						//}
						//if (battleScore - reportedScore < 1.1f)
						//{
						//	speedB = 1.5f / 8.0f;
						//	break;
						//}
						if (speedB < 1)
						{
							speedB = 0;

							reportedScore = battleScore;
						}

					}

					if (speed < minSpeed) speed = minSpeed;
					speed = speedB * 7.0f;
					reportedScore += delta * speed;

					if (speed < 0) speed = initSp * 0.001f;
				}
			}

			//reportedScore = (timeInState*timeInState-0.15f) * 500;
			if (timeInState < 0.4f) reportedScore = 0;
			if (reportedScore < 0) reportedScore = 0;
			if (reportedScore >= BattlGetScore())
			{
				reportedScore = BattlGetScore();

				next = gameover::complete;
			}

			if (wantToProgress)
			{
				next = gameover::complete;
			}
		}
		break;
		case gameover::complete:
			scale = 3;
			if (wantToProgress)
			{
				return -1;
			}
			if (fmod(timeInState, 0.3f * 2) > 0.3f)
			{
				c = color_bright;
			}
			else
			{
				c = color_bright2;
			}
			break;
		default:
			break;
		}

		if (next != st)
		{
			st = next;
			if (st == complete)
			{
				//ScreentintSDL({255,255,255,255}, 0.2f);
				PlaySound(SoundEnum::enemydead);
			}
		}

		DrawTextSDL(TextRender(finishText.c_str(), 150, (int)height*0.3f, { 255,255,255,255 }, 2, 0));
		DrawTextSDL(TextRender("Score", 150, (int)height*0.5f, { 255,255,255,255 }, 2, 0));
		DrawTextSDL(TextRender(std::to_string((int)reportedScore).c_str(), 150, (int)height*0.6f, c, scale, 0));
		return 1;

	}

}

BattleResult BattleLoop(bool pressedEnter)
{

	auto r = BattleLoop_Loop(pressedEnter);
	if (r == BattleResult::closeGame)
	{
		gameRunning = false;
	}
	return r;
}

ParticleExecuter particleExecuterMulti;

bool particleRunning = true;

int RunParticleUpdate(void* data)
{
	auto previous = std::chrono::steady_clock::now();
	while (particleRunning)
	{
		double deltaTime = (std::chrono::steady_clock::now() - previous).count() / 1000000000;
		particleExecuterMulti.Update(deltaTime);
		auto previous = std::chrono::steady_clock::now();
	}
	return 1;
}



void SaveData()
{
	SDL_Log("save");
	currentSaveVersion = latestSaveVersion;
	bool s = persistence::SaveStart(saveFilename);
	if (s == false) {
		return;
	}
	persistence::WriteInt(latestSaveVersion);
	persistence::WriteInt(storyProgress);
	persistence::WriteInt((int)storyGoalP);
	if (enemyDefeatedAmount_pers.size() != 30)
	{
		SDL_Log("Be careful, amount is off on save data!");
	}
	for (size_t i = 0; i < enemyDefeatedAmount_pers.size(); i++)//30
	{
		persistence::WriteInt(enemyDefeatedAmount_pers[i]);
	}
	for (size_t i = 0; i < 100; i++)
	{
		if (storiesSeen.size() > i)
		{
			persistence::WriteInt(storiesSeen[i]);
		}
		else
		{
			persistence::WriteInt(-1);
		}

	}

	if (latestSaveVersion >= 5)
	{
		for (size_t i = 0; i < powerup::maxJoinableSpiritAmountCeiling; i++)
		{
			if (i < powerup::joinedSpirits.size())
			{
				persistence::WriteInt(powerup::joinedSpirits[i].enemyId);
				persistence::WriteInt(powerup::joinedSpirits[i].level);
				persistence::WriteInt(powerup::joinedSpirits[i].xp);
				persistence::WriteInt(powerup::joinedSpirits[i].powerUpIds[0]);
				persistence::WriteInt(powerup::joinedSpirits[i].powerUpIds[1]);
				persistence::WriteInt(powerup::joinedSpirits[i].powerUpIds[2]);
				//if (latestSaveVersion >= 8)
				//{
				//	persistence::WriteInt(powerup::joinedSpirits[i].battlePowerBase * 10);
				//	persistence::WriteInt(powerup::joinedSpirits[i].battlePowerPerLevel * 10);
				//}
				//if (latestSaveVersion >= 11 && latestSaveVersion <= 12)
				//{
				//	persistence::WriteInt(powerup::joinedSpirits[i].hpBase * 10);
				//	persistence::WriteInt(powerup::joinedSpirits[i].hpPerLevel * 10);
				//	persistence::WriteInt(1); //max level legacy
				//}
				if (latestSaveVersion >= 13)
				{
					persistence::WriteInt(powerup::joinedSpirits[i].quality);
					persistence::WriteInt(powerup::joinedSpirits[i].farmlevel);
					persistence::WriteInt(powerup::joinedSpirits[i].farmxp);
				}

			}
			else
			{
				persistence::WriteInt(-1);
				persistence::WriteInt(-1);
				persistence::WriteInt(-1);
				persistence::WriteInt(-1);
				persistence::WriteInt(-1);
				persistence::WriteInt(-1);
				if (latestSaveVersion >= 13)
				{
					persistence::WriteInt(-1);
					persistence::WriteInt(-1);
					persistence::WriteInt(-1);
				}
			}
		}
		for (size_t i = 0; i < powerup::maxActiveSpiritCeiling; i++)
		{
			if (i < powerup::activeSpirits.size())
			{
				persistence::WriteInt(powerup::activeSpirits[i]);
			}
			else
			{
				persistence::WriteInt(-1);
			}
		}
		if (latestSaveVersion >= 14)
		{

			for (size_t i = 0; i < powerup::maxFarmSpiritCeiling; i++)
			{
				if (i < farm::spiritsInFarm.size())
				{
					persistence::WriteInt(farm::spiritsInFarm[i]);
				}
				else
				{
					persistence::WriteInt(-1);
				}
			}
		}
	}

	if (latestSaveVersion >= 7)
	{
		persistence::WriteInt(SpiritXPScreen::totalXP);
	}
	if (latestSaveVersion >= 10)
	{
		for (size_t i = 0; i < 100; i++)
		{
			if (VariableGP::variables.size() > i)
			{
				persistence::WriteInt(VariableGP::variables[i]);
			}
			else
			{
				persistence::WriteInt(0);
			}

		}
	}

	persistence::WriteBool(hasBattleData);
	if (hasBattleData)
	{
		std::vector<int> dataToSave = loadedBattleData;
		int battleDataSize = dataToSave.size();
		persistence::WriteInt(battleDataSize);
		for (size_t i = 0; i < dataToSave.size(); i++)
		{
			persistence::WriteInt(dataToSave[i]);
		}
	}

	persistence::SaveEnd();
	auto savedData = persistence::SaveDataAsString();
	auto incrementalXML = IncrementalSDL::SaveData();
	std::string savedDataBXML = "";
	{

		tinyxml2::XMLDocument xdoc;
		auto m = xdoc.NewElement("savedata");
		xdoc.InsertEndChild(m);
		auto ele = xdoc.NewElement("cutscenesseen");
		m->InsertEndChild(ele);
		for (size_t i = 0; i < persData.cutscenesSeen.size(); i++)
		{
			auto k = xdoc.NewElement("scene");
			k->SetAttribute("id", persData.cutscenesSeen[i].c_str());
			ele->InsertEndChild(k);
		}
		tinyxml2::XMLPrinter printer;
		xdoc.Print(&printer);
		std::string text = printer.CStr();
		savedDataBXML = text;
		FileUtils::WriteToFile(saveFilenameB, text);
	}

	savedData += '$';
	savedData += incrementalXML;
	savedData += '$';
	savedData += savedDataBXML;
	BrowserSave::LatestSave(savedData);
	persistence::Commit();
	//SDL_Log("Saved %d %d", latestSaveVersion, storyProgress);


}

void LoadSaveDataBXML(std::string text)
{
	persData = PersistentData();
	tinyxml2::XMLDocument xdoc;
	xdoc.Parse(text.c_str());
	auto ele = xdoc.FirstChild()->FirstChild();
	while (ele != NULL)
	{
		auto e = ele->ToElement();
		std::string n = e->Name();
		if (n == "cutscenesseen")
		{
			auto s = ele->FirstChild();
			while (s != NULL)
			{
				persData.cutscenesSeen.push_back(s->ToElement()->Attribute("id"));
				s = s->NextSibling();
			}
		}
		ele = ele->NextSibling();
	}
}


void LoadMasterDataString(std::string saveData)
{
	auto svs = split(saveData, '$');
	bool loaded = LoadDataFromString(svs[0]);
	if (loaded) 
	{
		IncrementalSDL::LoadData(svs.at(1), currentSaveVersion);
		if (svs.size() > 2)
		{
			LoadSaveDataBXML(svs[2]);
		}
		SaveData();
		nextState = ems_Title;
		uisystem::EventMessage("Loaded Data!");
	}
	else 
	{
		uisystem::EventMessage("Old Versions not supported");
	}
	
	
}


void LoadDataFromFiles()
{
	persistence::LoadStart(saveFilename);
	bool loaded = LoadDataRaw();
	if (loaded) 
	{
		
		auto s = FileUtils::ReadFromFile(saveFilenameB);
		if (s.size() > 0)
			LoadSaveDataBXML(s);
	}
	
	
}

void ResumeBattleOrGoToHub()
{

	if (hasBattleData)
	{
		nextState = ems_Battle;
	}
	else
	{
		state = ems_None;
		nextState = ems_Room;
		roomAlias = "hub";
	}
}

GameConfig gameConfig;

int mainA()
{
#ifdef GAMEB
	storyMessages.clear();
	gameConfig.gameStartcutscene = "sarabefore";
	gameConfig.hubMapFile = "assets/gamemapb.tmx";
	gameConfig.gameTitle = "Brisa Charm Sequel??";

	{
		DungeonMap::BattleGen bg;
		bg.groupEnemySpawn.push_back(EnemyGroupSpawn(-1, 99, 1, 0));
		bg.groupEnemySpawn.push_back(EnemyGroupSpawn(-1, 99, 1, 2));
		bg.waveNRange.Set(1, 3);
		gameConfig.mapData.battleGens.push_back(bg);
	}

	{
		DungeonMap::Spot spot;
		spot.pos.Set(0, -1);
		//spot.links.push_back(DungeonMap::Link(1, 0));
		gameConfig.mapData.spots.push_back(spot);
	}
	{
		DungeonMap::Spot spot;
		spot.pos.Set(0, 2);
		{
			DungeonMap::SpotEvent e;
			e.type = DungeonMap::SpotEventType::Cutscene;
			e.data = "sarabefore";
			spot.events.push_back(e);
		}

		gameConfig.mapData.spots.push_back(spot);
	}
	{
		DungeonMap::Spot spot;
		spot.pos.Set(0, 5);
		{
			DungeonMap::SpotEvent e;
			e.type = DungeonMap::SpotEventType::ResourceGate;
			e.dataNumber[0] = 1;
			e.dataNumber[1] = 100;
			spot.events.push_back(e);
		}
		gameConfig.mapData.spots.push_back(spot);
	}



#else
	gameConfig.gameStartcutscene = "gamebegin";
	gameConfig.hubMapFile = "assets/testmap.tmx";
	gameConfig.gameTitle = "Brisa Charm";
#endif
	if (storyMessages.size() == 0)
	{
		state = ems_Title;
		nextState = ems_Title;
	}
	DungeonMap::CurrentMap() = gameConfig.mapData;
	VariableGP::variables.resize(30);
	VariableGP::Init();
	{
		auto s = gameConfig.hubMapFile;
		tmxparser::TmxMap map;
		tmxparser::TmxReturn error = tmxparser::parseFromFile(s, &map, "assets/");
		MapRendering::RegisterMap(map, "hub");


	}
	{
		tmxparser::TmxMap map;
		tmxparser::TmxReturn error = tmxparser::parseFromFile("assets/tutomap.tmx", &map, "assets/");
		MapRendering::RegisterMap(map, "tutorial");
	}



	IgnoreOffsetOnLayerSDL(layerUI);
	IgnoreOffsetOnLayerSDL(layerUI2);

	Dialog::dialogBackColor = color_backgroundB;


	{
		const int s = 7;
		const int ss = 6;
		string txts[ss][s] = {
			{

			"Press WASD to move. \n Come to me.",
			"You can talk to people by touching them like that.\nTouch again to see more dialog.",
			"Press ESC to open configuration. You can also press m to mute and unmute audio",
			"Use the teleport to go off in your adventure.",
			"I have nothing else to say.\n If you talk to me again I'll start repeating myself.",
			},
			{
				"You can summon spirits in the leaf woods.",
				"Spirits give you various passive bonuses.",
				"Even if you are defeated in battle you don't lose your spirits.",
			},
			{
				"In here you can fight.",
				"You might feel like the battles are too challenging...",
				"You can always use the battle feature of the leaf woods instead!",
			},
			{
				"Iracema\nWelcome back, Sara",
				"Sara\n...",
			},
			{
				"Special thanks to Christoffer Lundberg and 'Azarlak' Alexander Johansson",
				"All the feedback makes a huge difference",
				"Sara\nFeedback?",
				"Sara\nWhat are you talking about?",
			},
			{
				"Sara\nThis is a space for spirits?",
				"It's the sanctuary, yes",
				"By putting spirits you can raise their sanctuary level",
				"Raising their sanctuary level raises their max level",
			},
		};

		for (size_t j = 0; j < ss; j++)
		{
			Dialog::DialogSeq d;
			for (size_t i = 0; i < s; i++)
			{
				Dialog::Dialog d2;
				if (txts[j][i].size() > 0)
				{
					d2.text = txts[j][i];
					d.dialogs.push_back(d2);
				}

			}
			Dialog::dialogDatabase.push_back(d);
		}

	}



	LoadDataFromFiles();
	IncrementalSDL::InitData();
	bool loadHappens = false;
	if (currentSaveVersion < 0)
	{
		auto localSave = BrowserSave::AttemptReadLocalStorage();
		if (localSave.size() > 0)
		{
			LoadMasterDataString(localSave);
			loadHappens = true;
		}

	}
	if (loadHappens == false)
	{
		IncrementalSDL::LoadData(currentSaveVersion);
	}
	persistence::LoadEndAll(); //clears all files before resaving


	if (VariableGP::variables[VariableGP::brisaVariables::tutoPortal] > 0)
	{
		roomAlias = "hub";
	}

	SaveData();

	IncrementalSDL::DataSetup();


	StatLeader::Init();
#ifndef GAMEB

	quest::Init();
#endif // 




	quest::currentQuest = VariableGP::variables[VariableGP::brisaVariables::questMain];

	for (size_t i = 0; i < quest::mainQuests.size(); i++)
	{
		for (size_t p = 0; p < quest::mainQuests[i].incrPrizeIdsToResolve.size(); p++)
		{
			auto id = IncrementalBrisa::resolveIds(quest::mainQuests[i].incrPrizeIdsToResolve[p]);
			quest::mainQuests[i].namePrize.push_back(IncrementalBrisa::entitiesInc[id].name);
			quest::mainQuests[i].incrPrizeIds.push_back(id);
		}
	}

	//IncrementalSDL::SaveData();

	storyRequirements.resize(storyMessages.size());
	//story requirements #req
#ifndef GAMEB
	{
		Requirement r;
		r.type = RequirementTypes::enemy_Defeated;
		r.n1 = 7;
		r.n2 = 1;
		r.description = "Defeat Boimokoi";
		storyRequirements[3].push_back(r);
	}
	{
		Requirement r;
		r.type = RequirementTypes::enemy_Defeated;
		r.n1 = 13;
		r.n2 = 1;
		r.description = "Defeat Caipora";
		storyRequirements[11].push_back(r);
	}
	{
		Requirement r;
		r.type = RequirementTypes::enemy_Defeated;
		r.n1 = 14;
		r.n2 = 1;
		r.description = "Defeat BoimokoiPhantomX";
		storyRequirements[14].push_back(r);
	}


#endif // !GAMEB





	srand(time(NULL));

	int rdmTest[4] = { 0,0,0,0 };
	for (size_t i = 0; i < 100; i++)
	{
		rdmTest[RandomRangeInt(0, 3)]++;
	}
	auto desired = CheckDesiredSize();
	if (desired.x > 0)
	{
		width = desired.x;
		height = desired.y;
	}

	Init_SDL(width, height);//SDのコードをスタート
	AtlasManager::LoadData("assets/dancesdl.atlas");
	IncrementalSDL::ImageData();
	SetBackgroundColorSDL(color_background);

	BattleLoop_BasicInit();
	BattleLoop_BattleInit();
	//auto img = AddImageSDL("assets/dancesdl.png");
	//img.GetData()->visible = false;

	//SDL_SetHint(SDL_HINT_RENDER_BATCHING, "1");

	if (storyProgress > 0)
	{
		nextState = ems_Title;

	}

	StoryBrisa::LoadCutscenes("assets/story/gamebegin.xml");
	StoryBrisa::LoadCutscenes("assets/story/sarabefore.xml");
	StoryBrisa::LoadCutscenes("assets/story/miguelbefore.xml");

	//if (VariableGP::variables[VariableGP::gameBeginChoice] == 0 && gameConfig.)
	if (!VectorUtils::VectorContains(persData.cutscenesSeen, gameConfig.gameStartcutscene))
	{
		StoryBrisa::StartCutscene(gameConfig.gameStartcutscene);
		nextState = ems_Cutscene;
	}



	LoadSounds();

	AddSound("assets/sounds/C.wav");
	AddSound("assets/sounds/D.wav");
	AddSound("assets/sounds/E.wav");
	AddSound("assets/sounds/F.wav");
	AddSound("assets/sounds/G.wav");
	AddSound("assets/sounds/A.wav");
	AddSound("assets/sounds/B.wav");
	AddSound("assets/sounds/De.wav");

	//StartScreenTransition();
	ReverseScreenTransition();

	//musicPlaying = false;


#ifdef __EMSCRIPTEN__

	EM_ASM(
		document.getElementById('spinner').hidden = true;
	document.addEventListener('click', function(){
		document.getElementById('canvas').focus();
	});
	);
	emscripten_set_main_loop(LoopRun, -1, 1);
	emscripten_set_main_loop_timing(EM_TIMING_RAF, 8);

#else
	while (gameRunning)//ゲームのマスターループ
		LoopRun();
	particleRunning = false;



	Quit_SDL();

#endif

	return 0;
}

int main(int argc, char* argv[])
{
#ifdef __DEBUG__
	drawDebug = true;
#endif

	auto dirName = "/savedata";
	persistence::Init(dirName);

#ifdef __EMSCRIPTEN__

#else
	mainA();
#endif

	return 1;
}


//#loop #mainloop
void LoopRun()
{
	deltaTimeSDL *= deltaMultiplier;
	if (deltaOverwrite >= 0)
	{
		deltaTimeSDL = deltaOverwrite;
	}

	bool reloadRoomState = false;
	Profiler::StartMaster();
	{
		bool enterPressed = IsKeyPressed(SDLK_RETURN) || IsKeyPressed(SDLK_KP_ENTER) || IsKeyPressed(SDLK_SPACE);
		bool enterDownFrame = IsKeyDownFrame(SDLK_RETURN) || IsKeyDownFrame(SDLK_KP_ENTER) || IsKeyDownFrame(SDLK_SPACE);
		bool enterDownFrameKBOnly = enterDownFrame;
		bool canvasFocused = true;


#ifdef __EMSCRIPTEN__
		canvasFocused = (bool)EM_ASM_INT(
			return document.getElementById('canvas') == document.activeElement;
		return false;
		);
#endif
		std::string saveData = BrowserSave::AttemptReadFile();
		if (saveData.size() > 0)
		{
			LoadMasterDataString(saveData);
		}

		int sdlloop = 0;
		if (canvasFocused == false && mobileDeviceSDL == false)
		{
			sdlloop = Loop_SDL();//SDL backend code
			DrawTextSDL(TextRender("Click to get focus!", 150, (int)height*0.3f, color_bright, 2, 0));
			return;
		}

		if (cursorPressedSDL)
		{
			enterPressed = true;
		}
		if (cursorDownFrameSDL)
		{
			enterDownFrame = true;
		}

		farm::Update();

		bool wantToAdvance = enterDownFrame;
		float timeStartFrame = currentTimeSDL;
		if (renderScreenTransition)
		{
			UpdateScreenTransition(deltaTimeSDL);
			if (renderScreenTransition)
				RenderScreenTransition();
		}
		bool runState = true;

		if (state != ems_Title)
		{
			bool configPress = false;
			if (IsKeyDownFrame(SDLK_ESCAPE) && state != ems_Incremental)
			{
				configPress = true;

			}
			if (!ConfigSDL::active)
			{
				uisystem::uiElement menuButton;
				menuButton.label = "Menu\nesc";
				menuButton.pos.Set(widthSDL - 60, heightSDL - 32);
				menuButton.size.Set(60, 32);
				uisystem::Draw(menuButton, layerUI2);
				if (menuButton.clicked)
				{
					configPress = true;
				}
			}


			if (configPress)
			{
				ConfigSDL::active = !ConfigSDL::active;
				ConfigSDL::resetLevel = 0;
			}
		}

		if (StoryBrisa::RequestedCutscene().size() > 0)
		{
			nextState = ems_Cutscene;
			StoryBrisa::StartCutscene(StoryBrisa::RequestedCutscene());
		}

		if (ConfigSDL::active)
		{
			runState = false;
			//if (state == ems_Room && roomAlias == "tutorial")
			if (VariableGP::variables[VariableGP::brisaVariables::gameBeginChoice] == 0)
				ConfigSDL::disableHub = true;
			else
			{
				ConfigSDL::disableHub = false;
			}
			sdlloop = Loop_SDL();
			ConfigSDL::RenderAndInput(enterDownFrameKBOnly);
			if (ConfigSDL::resetLevel >= 2)
			{
				persData = PersistentData();
				FileUtils::WriteToFile(saveFilenameB, "");
				persistence::Reset(saveFilename);
				LoadDataFromFiles();
				IncrementalSDL::ResetData();
				IncrementalSDL::LoadData(currentSaveVersion);

				ConfigSDL::resetLevel = 0;
				ConfigSDL::active = false;
				ConfigSDL::wantHub = false;
				StoryBrisa::StartCutscene(gameConfig.gameStartcutscene);
				nextState = ems_Cutscene;
				//ResumeBattleOrGoToHub();
			}
			if (ConfigSDL::wantHub)
			{

				roomAlias = "hub";
				nextState = ems_Room;
				ConfigSDL::resetLevel = 0;
				ConfigSDL::active = false;
				ConfigSDL::wantHub = false;
				reloadRoomState = true;
			}

		}

		if (IsKeyDownFrame(SDLK_m))
		{
			soundPlaying = !soundPlaying;
			musicPlaying = !musicPlaying;
			if (soundPlaying)
			{
				uisystem::EventMessage("Sound On");
			}
			else
			{
				StopMusicSDL();
				uisystem::EventMessage("Sound Off");
			}

			//uisystem::EventMessage();
		}

		uisystem::DrawEventMessages();

		if (quest::questClearCheckRequest)
		{
			quest::questClearCheckRequest = false;
			using namespace quest;
			if (quest::currentQuest < mainQuests.size())
			{
				if (CheckRequirements(mainQuests[currentQuest].reqs))
				{
					questClearFlag = true;
					for (size_t p = 0; p < quest::mainQuests[currentQuest].incrPrizeAmount.size(); p++)
					{
						IncrementalSDL::GetResource(
							mainQuests[currentQuest].incrPrizeIds[p],
							mainQuests[currentQuest].incrPrizeAmount[p]);
					}
					currentQuest++;
					VariableGP::variables[VariableGP::brisaVariables::questMain] = currentQuest;
				}
				else
				{
					questClearFlag = false;
				}
			}
		}

		timeOnState += deltaTimeSDL;

		if (StoryBrisa::GetStartCutsceneId().size() > 0)
		{
			if (!VectorUtils::VectorContains(persData.cutscenesSeen, StoryBrisa::GetStartCutsceneId()))
			{
				persData.cutscenesSeen.push_back(StoryBrisa::GetStartCutsceneId());
			}

			StoryBrisa::GetStartCutsceneId() = "";
		}

		if (IsKeyPressed(SDLK_o) && IsKeyPressed(SDLK_p))
		{
			if (IsKeyDownFrame(SDLK_j)) {
				drawDebug = !drawDebug;
				Profiler::debugCommand.clear();
			}
		}

		if (Profiler::enabled)
		{
			if (IsKeyDownFrame(SDLK_ESCAPE))
			{
				renderOnSDL = !renderOnSDL;
			}
		}

		if (runState)
		{

			//current state loop
			switch (state)
			{
			case ems_Choice:
			{

				auto result = BattleLoop(enterDownFrame);
				BattleLoop_Render(enterDownFrame);
				BattleChoices::RenderChoices();
				BattleChoices::Update();

				if (BattleChoices::choice != BattleChoices::ChoicesE::None &&StoryBrisa::GetExecutionData().waitingForChoice)
				{
					StoryBrisa::ChoiceDone();
					nextState = ems_Cutscene;
				}

				if (BattleChoices::choice == BattleChoices::ChoicesE::StoryLater)
				{
					nextState = ems_Battle;
				}
				if (BattleChoices::choice == BattleChoices::ChoicesE::StoryWatch)
				{
					nextState = ems_MainStory;
				}
				if (BattleChoices::choice == BattleChoices::ChoicesE::BackToHub)
				{
					nextState = ems_Room;
					if (hasBattleData)
					{
						BattleLoop_BasicInit();
						BattleLoop_BattleInit();
						BattleLoop_LoadData(loadedBattleData, currentSaveVersion);
					}
					int fullXP = BattleLoop_BankedXP();
					if (fullXP > 0)
					{
						nextState = ems_SpiritXP;
						nextStateAfterSpiritXP = ems_Room;
					}
				}
				if (BattleChoices::choice == BattleChoices::ChoicesE::StoryCatchup)
				{
					nextState = ems_MainStory;
				}
				if (BattleChoices::choice == BattleChoices::ChoicesE::StoryContinue)
				{
					nextState = ems_MainStory;
				}

			}
			break;
			case ems_Title:
			{
				int xLeft = widthSDL / 2 - 140;
				int xRight = widthSDL - xLeft;
				{
					auto color = SDL_Color{ 255,255,255,255 };
					if (soundPlaying == false) color = color_darkcool;
					ImageRegionDraw ird;
					ird.x = widthSDL * 0.9f;
					ird.y = 32;
					ird.color = color;
					ird.ir = AtlasManager::GetImage("volume");
					if (IsTouch(ird, 40))
					{
						ird.scaleX = 2;
						ird.scaleY = 2;
					}
					AddRegionDraw(ird, 3);
					DrawTextSDL(TextRender("-m-", ird.x, ird.y + 5, color));



					if (IsTouchDown(ird, 40))
					{
						wantToAdvance = false;
						soundPlaying = !soundPlaying;
						musicPlaying = !musicPlaying;
						if (soundPlaying) PlaySound(SoundEnum::enemydamage);
					}
				}
				SDL_Color titleColor = { 255,255,255,255 };
				titleColor = color_bright2;
				float scale = 2;
				if (fmod(currentTimeSDL, 0.4f) < 0.1f)
				{
					scale = 2.1f;
					//titleColor = color_bright;
				}
				int y = (int)height*0.3f - 32;

				DrawTextSDL(TextRender(gameConfig.gameTitle.c_str(), widthSDL / 2, (int)height*0.3f, titleColor, scale, 0));
				DrawTextSDL(TextRender("ブリザチャーム", widthSDL / 2, (int)height*0.3f + 30, titleColor, 1, 0));
				DrawTextSDL(TextRender("vers 0.12", widthSDL / 2, (int)height*0.3f + 45, titleColor, 1, 0));
				auto color_control = color_white;
				if (mobileDeviceSDL)
				{
					DrawTextSDL(TextRender("DRAG to move", widthSDL / 2,
						(int)height*0.5f, color_control, 1, 0));
					DrawTextSDL(TextRender("TOUCH to start", widthSDL / 2,
						(int)height*0.5f + 16, color_control, 1, 0));
				}
				else
				{
					DrawTextSDL(TextRender("WASD to move (or arrows)", widthSDL / 2,
						(int)height*0.5f, color_control, 1, 0));
					DrawTextSDL(TextRender("ENTER to start", widthSDL / 2,
						(int)height*0.5f + 16, color_control, 1, 0));

				}

				auto color_creditA = color_whiteT;
				auto color_creditB = color_mid_warm;
				auto color_creditC = color_white;
				DrawTextSDL(TextRender("A Game by Pidroh", widthSDL / 2,
					(int)height*0.7f - 24, color_creditA, 1, 0));
				DrawTextSDL(TextRender("Developer", xLeft,
					(int)height*0.7f, color_creditB, 1, -1));
				DrawTextSDL(TextRender("Artist", xLeft,
					(int)height*0.7f + 16 * 1, color_creditB, 1, -1));
				//DrawTextSDL(TextRender("Special Thanks", xLeft,
				//	(int)height*0.7f + 16 * 2, { 255,255,255,255 }, 1, -1));
				DrawTextSDL(TextRender("Musician", xLeft,
					(int)height*0.7f + 16 * 3, color_creditB, 1, -1));
				DrawTextSDL(TextRender("Sound Effects", xLeft,
					(int)height*0.7f + 16 * 4, color_creditB, 1, -1));
				DrawTextSDL(TextRender("Pedro Gabriel Fonteles Furtado", xRight,
					(int)height*0.7f + 16 * 0, color_creditC, 1, 1));
				DrawTextSDL(TextRender("Pedro", xRight,
					(int)height*0.7f + 16 * 1, color_creditC, 1, 1));
				//DrawTextSDL(TextRender("Rodrigo Khuni", xRight,
				//	(int)height*0.7f + 16 * 2, { 255,255,255,255 }, 1, 1));
				DrawTextSDL(TextRender("Juhani Junkala", xRight,
					(int)height*0.7f + 16 * 3, color_creditC, 1, 1));
				DrawTextSDL(TextRender("phoenix1291", xRight,
					(int)height*0.7f + 16 * 4, color_creditC, 1, 1));
				//phoenix1291

				bool debugTime = false;
				float timeStartTitle = currentTimeSDL;
				auto timeStartTitleB = std::chrono::steady_clock::now();;
				sdlloop = Loop_SDL();//SDL backend code

				float xURL = 0;
				{
					auto discord = AtlasManager::GetImage("discord");
					ImageRegionDraw ird;
					ird.ir = discord;
					ird.scaleX = 0.17f;
					ird.scaleY = 0.17f;
					ird.pivotX = 0;
					ird.pivotY = 0.5f;
					ird.color = { 200,200,200,200 };
					ird.x = xURL;
					ird.y = 32;
					xURL += ird.scaleX * ird.ir.origin.w;
					AddRegionDraw(ird, 3);
					AddWebsiteURLClickRegion(ird, "https://discord.gg/AtGrxpM");

				}
				{
					auto pidroh = AtlasManager::GetImage("pidroh");
					ImageRegionDraw ird;
					ird.ir = pidroh;
					ird.scaleX = 1.0f;
					ird.scaleY = 1.0f;
					ird.pivotX = 0;
					ird.pivotY = 0.5f;
					ird.x = xURL;
					ird.y = 32;
					ird.color = { 200,200,200,200 };
					AddRegionDraw(ird, 3);
					AddWebsiteURLClickRegion(ird, "http://www.pidroh.com");
				}


				auto timeSDLEnd = std::chrono::steady_clock::now();
				//float timeSDLEnd = std::chrono::steady_clock::now();
				if (debugTime)
				{
					SDL_Log("SDL Loop Time %f", (double)(std::chrono::steady_clock::now() - timeStartTitleB).count() / 1000000);
					//SDL_Log("SDL Loop time %f", timeSDLEnd - timeStartTitleB);`
				}

				float frameRate = 1.0f / deltaTimeSDL;
				if (wantToAdvance && renderScreenTransition == false)
				{
					StartScreenTransition();
					PlaySound(SoundEnum::titlego);

				}
				if (overScreenTransition && renderScreenTransition)
				{
					ReverseScreenTransition();
					if (VariableGP::variables[VariableGP::brisaVariables::gameBeginChoice] == 2)
					{
						nextState = ems_Incremental;
					}
					else
					{
						nextState = ems_Room;
						roomAlias = "hub";
						if (hasBattleData)
						{
							BattleLoop_BasicInit();
							BattleLoop_BattleInit();
							BattleLoop_LoadData(loadedBattleData, currentSaveVersion);
						}
						int fullXP = BattleLoop_BankedXP();
						if (fullXP > 0)
						{
							nextState = ems_SpiritXP;
							nextStateAfterSpiritXP = ems_Room;
						}
					}


				}
				particleExecuterMulti.Update(deltaTimeSDL);
				auto timeUpdateParticle = std::chrono::steady_clock::now();
				if (debugTime)
				{
					//SDL_Log("SDL Particle Update %f", std::chrono::steady_clock::now() - timeSDLEnd);
					SDL_Log("SDL Particle Update %f", (double)(std::chrono::steady_clock::now() - timeSDLEnd).count() / 1000000);
				}
				//particleExecuterMulti.Render();
				if (debugTime)
				{
					SDL_Log("SDL Particle Render %f", (double)(std::chrono::steady_clock::now() - timeUpdateParticle).count() / 1000000);
				}
			}

			break;
			case ems_Room:
			{
				//MapRendering::Render();
				auto result = BattleLoop(enterDownFrame);

				if (powerup::spiritWantToShow >= 0)
				{
					BattleLoop_HideHero(0.1f);
					BattleLoop_HeroCamera(false);

					if (timeOnState >= 10.0f || enterDownFrame)
					{
						nextState = ems_Incremental;
						BattleLoop_HideHero(-1);
						powerup::spiritWantToShow = -1;
					}
				}
				else
				{
					farm::TryShowSpiritLevelUp();
				}


				int posOff = 0;
				if (deadenemypositionsForRoom.size() > 0 && false) {

					for (size_t i = 0; i < enemyDefeatedAmount_pers.size(); i++)
					{
						for (size_t j = 0; j < enemyDefeatedAmount_pers[i]; j++)
						{

							int size = BattleLoop_GetEnemySize(i);
							size = 8;
							if (fmod(currentTimeSDL, 0.4f) > 0.2f)
							{
								size += 2;
							}
							int x = (int)deadenemypositionsForRoom.at(posOff).x;
							int y = (int)deadenemypositionsForRoom.at(posOff).y;
							AddRegionDraw(
								ImageRegionDraw::ImageRegionDrawXYColor(
									BattleLoop_EnemyMainRegion(i),
									x, y, color_enemyDead), 0);
							//AddRectangleS({ (int)deadenemypositionsForRoom.at(posOff).x-size/2,(int)deadenemypositionsForRoom.at(posOff).y - size / 2,size, size }, color_enemyDead, 0);
							posOff++;
							if (j > 10)
							{
								break;
							}
						}
					}

				}
				MapRendering::Render();
				RoomCode::UpdateRoom();
				BattleLoop_Render(enterDownFrame);
				bool renderNewBattleText = false;
				bool renderResumeBattleText = false;
				const int txts = 3;
				bool already[txts] = { false,false };
				bool wannaSave = false;

				if (roomAlias == "hub") {

					if (powerup::buttons.size() < powerup::joinedSpirits.size())
					{
						powerup::buttons.resize(powerup::joinedSpirits.size());
						for (size_t i = 0; i < powerup::buttons.size(); i++)
						{
							powerup::buttons[i].label = "Rebirth";
						}
					}
				}
				if (roomAlias == "tutorial") {
					if (VariableGP::variables[VariableGP::brisaVariables::tutoPortal] == 0)
					{
						RoomCode::noDrawMetas.push_back("exittutorial");
					}
					else
					{
						if (RoomCode::collidingMeta == "exittutorial")
						{
							roomAlias = "hub";
							reloadRoomState = true;
						}
					}
				}

				int spiritToDraw = -1;
				int spiritToDrawReplace = -1;

				int yDisSp = 32;
				const float offsetSpiritY = 40;
				auto joinSpiritPos = RoomCode::GetMetaPositionWorld("joinedspirit");
				if (joinSpiritPos.x >= 0)
				{

					if (!powerup::CanASpiritJoin())
					{
						DrawTextSDL(TextRender("MAXED OUT", joinSpiritPos.x, joinSpiritPos.y - 50), 5);
					}
					int spiritsJoinedN = 0;
					for (size_t i = 0; i < powerup::joinedSpirits.size(); i++)
					{
						if (powerup::joinedSpirits[i].enemyId >= 0)
						{
							spiritsJoinedN++;
						}
					}
					if (spiritsJoinedN > 0)
						DrawTextSDL(TextRender("Reserve\nSpirits", joinSpiritPos.x, joinSpiritPos.y - 30, color_white), 5);
					for (size_t i = 0; i < powerup::joinedSpirits.size(); i++)
					{
						powerup::buttons[i].active = false;
						for (size_t act = 0; act < powerup::activeSpirits.size(); act++)
						{
							if (powerup::activeSpirits[act] == i)
							{
								goto skippowerup;
							}
						}
						for (size_t act = 0; act < farm::spiritsInFarm.size(); act++)
						{
							if (farm::spiritsInFarm[act] == i)
							{
								goto skippowerup;
							}
						}
						{

							int eid = powerup::joinedSpirits[i].enemyId;
							if (eid < 0) goto skippowerup;

							int perline = 8;
							auto ximg = joinSpiritPos.x + (i / perline) * 100;
							auto yimg = joinSpiritPos.y + offsetSpiritY + (i%perline) * yDisSp;
							auto ird = ImageRegionDraw::ImageRegionDrawXYColor(
								BattleLoop_EnemyMainRegion(eid),
								ximg, yimg, color_enemyDead);
							auto heroRect = BattleLoop_HeroRect();
							if (i == powerup::spiritJoinedTouched)
							{


								for (size_t interButton = 0; interButton < 2; interButton++)
								{
									int offset = 34;
									ird.x += offset - interButton * offset * 3;
									AddRegionDraw(ird, 6);
									std::string label = "Battle";
									if (interButton == 0) label = "Sanctuary";
									auto tr = TextRender(label.c_str(), ird.x, ird.y + 18);
									tr.color_Border = color_darkgreen;
									DrawTextSDL(tr);

									SDL_Rect imgRect = { (int)ird.x - 8, (int)ird.y - 8, 16,16 };
									auto heroRO = BattleLoop_HeroRectLastFrame();
									if (!SDL_HasIntersection(&heroRO, &imgRect)) {
										if (SDL_HasIntersection(&heroRect, &imgRect))
										{
											int result = -1;
											std::string errorM = "Too many spirits equipped";
											if (interButton == 1)
											{
												result = powerup::EquipSpirit(i);
											}
											if (interButton == 0)
											{
												result = farm::JoinFarm(i);
												errorM = "The sanctuary is full";
											}

											if (result >= 0)
											{
												PlaySound(SoundEnum::uiconfirm);
												wannaSave = true;
											}
											else
											{
												PlaySound(SoundEnum::uibad);
												uisystem::EventMessage(errorM);
											}
											powerup::spiritJoinedTouched = -1;




										}
									}
								}

								goto skippowerup;
							}
							//powerup::buttons[i].active = powerup::joinedSpirits[i].level >= powerup::joinedSpirits[i].maxLevel;
							powerup::buttons[i].active = true;
							if (externalinteg::GetValue(externalinteg::idenum::spiritPresent) == 1)
							{
								powerup::buttons[i].active = false;
							}

							powerup::buttons[i].pos.Set(ximg + 25, yimg - 5);

							//auto imgRect = GetSDLRectFromImageRegion(ird);
							SDL_Rect imgRect = { (int)ximg - 8, (int)yimg - 8, 16,16 };
							auto heroRO = BattleLoop_HeroRectLastFrame();
							if (!SDL_HasIntersection(&heroRO, &imgRect)) {
								if (SDL_HasIntersection(&heroRect, &imgRect))
								{

									powerup::spiritJoinedTouched = i;
									/*int result = powerup::EquipSpirit(i);
									if (result >= 0)
									{
										PlaySound(SoundEnum::uiconfirm);
										wannaSave = true;
									}
									else
									{
										PlaySound(SoundEnum::uibad);
										uisystem::EventMessage("Too many spirits equipped");
									}*/


								}
							}

							if (RectOverlapSDL(heroRect, imgRect, 20, 3))
							{
								spiritToDraw = i;
								int equipId = powerup::EquippedSpiritWithSameEnemyId(spiritToDraw);
								if (equipId >= 0)
								{
									spiritToDrawReplace = powerup::activeSpirits[equipId];
								}

							}


							AddRegionDraw(ird, 6);

							if (i == powerup::spiritWantToShow)
							{

								BattleLoop_CenterCamera(Vector2D(ird.x, ird.y));
							}
						}

					skippowerup:;
					}
				}

				uisystem::UpdateAndRender(powerup::buttons);
				for (size_t i = 0; i < powerup::buttons.size(); i++)
				{
					if (powerup::buttons[i].actionHappenRequest)
					{
						powerup::buttons[i].confirming = false;
						powerup::buttons[i].actionHappenRequest = false;
						externalinteg::SpiritRebirth(powerup::joinedSpirits[i].enemyId);
						powerup::joinedSpirits[i].enemyId = -1;

						wannaSave = true;
					}
				}

				auto discordP = RoomCode::GetMetaPositionWorld("discordbutton");
				if (discordP.x >= 0)
				{
					auto discord = AtlasManager::GetImage("discord");
					ImageRegionDraw ird;
					ird.ir = discord;
					ird.scaleX = 0.17f;
					ird.scaleY = 0.17f;
					ird.pivotX = 0;
					ird.pivotY = 0.5f;
					ird.color = { 200,200,200,200 };
					ird.x = discordP.x;
					ird.y = discordP.y;
					AddRegionDraw(ird, 3);
					AddWebsiteURLClickRegion(ird, "https://discord.gg/AtGrxpM");
				}


				std::string metasS[] = { "equippedspirit", "farmspirit" };
				std::string metasSlab[] = { "Active\nSpirits", "Sanctuary" };

				int metaS = 0;

				auto equippedSpiritPos = RoomCode::GetMetaPositionWorld(metasS[metaS].c_str());
				if (equippedSpiritPos.x >= 0)
				{
					if (powerup::activeSpirits.size() > 0)
						DrawTextSDL(TextRender(metasSlab[metaS].c_str(), equippedSpiritPos.x, equippedSpiritPos.y - 30, color_white), 5);
					for (size_t i = 0; i < powerup::activeSpirits.size(); i++)
					{

						int eid = powerup::joinedSpirits[powerup::activeSpirits[i]].enemyId;
						if (eid < 0)
						{
							powerup::activeSpirits.erase(powerup::activeSpirits.begin() + i);
							i--;
							continue;
						}

						auto yimg = equippedSpiritPos.y + offsetSpiritY + i * yDisSp;//powerup::activeSpirits[i] * yDisSp;
						auto ird = ImageRegionDraw::ImageRegionDrawXYColor(
							BattleLoop_EnemyMainRegion(eid),
							equippedSpiritPos.x, yimg, color_enemyDead);

						auto heroRect = BattleLoop_HeroRect();
						auto imgRect = GetSDLRectFromImageRegion(ird);
						auto ximg = equippedSpiritPos.x;
						imgRect = { (int)ximg - 8,  (int)yimg - 8, 16,16 };
						if (RectOverlapSDL(heroRect, imgRect, 20, 3))
						{
							spiritToDraw = powerup::activeSpirits[i];
						}
						auto heroRO = BattleLoop_HeroRectLastFrame();
						if (!SDL_HasIntersection(&heroRO, &imgRect))
							if (SDL_HasIntersection(&heroRect, &imgRect))
							{
								powerup::activeSpirits.erase(powerup::activeSpirits.begin() + i);
								i--;
								continue;
							}

						AddRegionDraw(ird, 6);

						if (powerup::activeSpirits[i] == powerup::spiritWantToShow)
						{

							BattleLoop_CenterCamera(Vector2D(ird.x, ird.y));
						}
					}
				}

				metaS = 1;

				equippedSpiritPos = RoomCode::GetMetaPositionWorld(metasS[metaS].c_str());
				if (equippedSpiritPos.x >= 0)
				{
					if (farm::spiritsInFarm.size() > 0)
						DrawTextSDL(TextRender(metasSlab[metaS].c_str(), equippedSpiritPos.x, equippedSpiritPos.y - 30, color_white), 5);
					for (size_t i = 0; i < farm::spiritsInFarm.size(); i++)
					{

						int eid = powerup::joinedSpirits[farm::spiritsInFarm[i]].enemyId;
						if (eid < 0)
						{
							farm::spiritsInFarm.erase(farm::spiritsInFarm.begin() + i);
							i--;
							continue;
						}

						auto yimg = equippedSpiritPos.y + offsetSpiritY + i * yDisSp;//farm::spiritsInFarm[i] * yDisSp;
						auto ird = ImageRegionDraw::ImageRegionDrawXYColor(
							BattleLoop_EnemyMainRegion(eid),
							equippedSpiritPos.x, yimg, color_enemyDead);

						auto heroRect = BattleLoop_HeroRect();
						auto imgRect = GetSDLRectFromImageRegion(ird);
						auto ximg = equippedSpiritPos.x;
						imgRect = { (int)ximg - 8,  (int)yimg - 8, 16,16 };
						if (RectOverlapSDL(heroRect, imgRect, 20, 3))
						{
							spiritToDraw = farm::spiritsInFarm[i];
						}
						auto heroRO = BattleLoop_HeroRectLastFrame();
						if (!SDL_HasIntersection(&heroRO, &imgRect))
							if (SDL_HasIntersection(&heroRect, &imgRect))
							{
								farm::spiritsInFarm.erase(farm::spiritsInFarm.begin() + i);
								i--;
								continue;
							}

						AddRegionDraw(ird, 6);

						if (farm::spiritsInFarm[i] == powerup::spiritWantToShow)
						{

							BattleLoop_CenterCamera(Vector2D(ird.x, ird.y));
						}
					}
				}


				if (powerup::spiritWantToShow >= 0)
				{
					spiritToDraw = powerup::spiritWantToShow;
				}


				if (spiritToDrawReplace >= 0)
				{
					uisystem::DrawSpiritInfo(powerup::joinedSpirits[spiritToDrawReplace], Vector2D(widthSDL / 2, 0));
				}

				if (spiritToDraw >= 0)
				{
					uisystem::DrawSpiritInfo(powerup::joinedSpirits[spiritToDraw], Vector2D(widthSDL / 2, 170));
				}

				RoomCode::noDrawMetas.push_back("equippedspirit");
				RoomCode::noDrawMetas.push_back("farmspirit");
				RoomCode::noDrawMetas.push_back("joinedspirit");
				RoomCode::noDrawMetas.push_back("spawn");
				RoomCode::noDrawMetas.push_back("NPC1");
				RoomCode::noDrawMetas.push_back("NPC2");
				RoomCode::noDrawMetas.push_back("NPC3");
				RoomCode::noDrawMetas.push_back("NPC4");
				RoomCode::noDrawMetas.push_back("discordbutton");

				std::string texts[txts] = { "Into the Forest", "Resume", "Leaf Woods" };
				std::string metaForText[txts] = { "newbattle", "resumebattle", "incremental" };
				if (hasBattleData == false)
				{
					already[1] = true;
					RoomCode::noDrawMetas.push_back(metaForText[1]);
				}
				else
				{
					already[0] = true;
					RoomCode::noDrawMetas.push_back(metaForText[0]);
				}

				auto instruction = "WASD to move";
				if (mobileDeviceSDL)
					instruction = "Touch and drag to move";
				TextRender tr(instruction);
				tr.x = widthSDL / 2;
				tr.y = heightSDL - 80;
				//tr.color_Border = { 0,0,0,255 };
				DrawTextSDL(tr, layerUI);

				for (size_t i = 0; i < RoomCode::nearMetas.size(); i++)
				{
					for (size_t t = 0; t < txts; t++)
					{
						if (already[t] == false && RoomCode::nearMetas[i] == metaForText[t])
						{
							already[t] = true;
							TextRender tr(texts[t].c_str());
							tr.x = RoomCode::nearMetasPos[i].x;
							tr.y = RoomCode::nearMetasPos[i].y;
							tr.color_Border = { 0,0,0,255 };
							DrawTextSDL(tr);
						}
					}

				}
				if (RoomCode::collidingMeta == "newbattle" && hasBattleData == false)
				{
					nextState = ems_Battle;
					hasBattleData = false;
					PlaySound(SoundEnum::enterplace);
				}
				if (RoomCode::collidingMeta == "incremental")
				{
					nextState = ems_Incremental;
					PlaySound(SoundEnum::enterplace);
				}
				if (RoomCode::collidingMeta == "resumebattle" && hasBattleData)
				{
					PlaySound(SoundEnum::enterplace);
					nextState = ems_Battle;
				}
				if (result == BattleResult::closeGame)
				{
					sdlloop = -1;
				}
				if (wannaSave)
				{
					SaveData();
				}
			}
			break;
			case ems_Battle:
			{

				auto result = BattleLoop(enterDownFrame);
				BattleLoop_Render(enterDownFrame);
				int xp = BattleLoop_BankedXP();
				BattleLoop_BankedXP() = 0;
				if (bd.incrementalprizeIds.size() > 0)
				{
					for (size_t i = 0; i < bd.incrementalprizeIds.size(); i++)
					{
						std::string msg;
						auto id = bd.incrementalprizeIds[i];
						msg = IncrementalBrisa::entitiesInc[id].name;
						msg += " +";
						msg += std::to_string(bd.incrementalprizeAmounts.at(i));
						uisystem::EventMessage(msg);
						IncrementalBrisa::entitiesInc[id].value += bd.incrementalprizeAmounts.at(i);
						BattleLoop_ShowBattleTextOnHero(msg);

					}
					bd.incrementalprizeAmounts.clear();
					bd.incrementalprizeIds.clear();
				}
				if (powerup::activeSpirits.size() > 0)
				{
					SpiritXPScreen::totalXP += xp;
				}

				if (BattleLoop_CheckWantGiveResource() >= 0)
				{
					auto txt = IncrementalSDL::BattleResource(BattleLoop_CheckWantGiveResource());
					BattleLoop_CheckWantGiveResource() = -1;
					BattleLoop_ShowBattleTextOnHero(txt);

				}
				if (BattleLoop_WantToSave())
				{
					loadedBattleData = BattleLoop_GetDataToSave(latestSaveVersion);
					hasBattleData = true;
					SaveData();
				}
				bool advanceStory = false;
				externalinteg::ReportWave(BattleLoop_WaveMain());
				if (result == BattleResult::storyWantToUnlock)
				{
					ReverseScreenTransition();
					//state = ems_Title;
					timeInStoryCharge = 0;
					nextState = ems_StoryCharge;
					advanceStory = true;
				}
				bool gameOver = false;
				if (result == BattleResult::clear)
				{
					finishText = "Clear";
					gameOver = true;
					if (bd.id == "maingame")
					{
						IncrementalSDL::MainGameClear();
					}

				}
				if (result == BattleResult::nextLinkRoom)
				{
					state = ems_None;
					nextState = ems_Battle;
				}
				if (result == BattleResult::spotProcessStart)
				{
					DungeonMap::Progress().currentSpotEvent = 0;
					DungeonMap::Progress().processingSpot = true;
					nextState = ems_MapSpot;

				}
				if (result == BattleResult::gameover)
				{
					finishText = "Gameover";
					gameOver = true;
				}
				if (gameOver)
				{
					VariableGP::UpdateIfHigher(VariableGP::brisaVariables::statBestScore, BattlGetScore());

					advanceStory = true;
					ReverseScreenTransition();
					hasBattleData = false;
					loadedBattleData.clear();

					//nextState = ems_Gameover;

					nextStateAfterSpiritXP = ems_Gameover;
					nextState = ems_SpiritXP;



					SaveData();
				}
				if (advanceStory)
				{
					storyGoalP_Last = storyGoalP;
					battleDuration = currentTimeSDL - battleStartTime;
					storyGoalP += battleDuration;
				}
				if (result == BattleResult::closeGame)
				{
					sdlloop = -1;
				}
			}

			break;
			case ems_MapSpot:
			{
				auto spot = DungeonMap::CurrentMap().spots.at(DungeonMap::Progress().latestSpot);
				while (spot.events.size() > DungeonMap::Progress().currentSpotEvent)
				{
					auto e = spot.events[DungeonMap::Progress().currentSpotEvent];

					switch (e.type)
					{
					case DungeonMap::SpotEventType::Cutscene:
						if (!IsCutsceneSeen(e.data))
						{
							StoryBrisa::StartCutscene(e.data);
							nextState = ems_Cutscene;
							break;
						}
						else
						{
							DungeonMap::Progress().currentSpotEvent++;
						}

						break;
					default:
						break;
					}
				}
				if ((spot.events.size() <= DungeonMap::Progress().currentSpotEvent))
				{
					nextState = ems_Battle;
				}
			}
			break;
			case ems_Incremental:
				IncrementalSDL::Loop();
				IncrementalSDL::Render();
				if (powerup::spiritWantToShow >= 0)
				{
					roomAlias = "hub";
					nextState = ems_Room;
				}
				
				if (externalinteg::wantSeeXP) {
					externalinteg::wantSeeXP = false;
					int fullXP = SpiritXPScreen::totalXP;
					if (fullXP > 0)
					{
						nextState = ems_SpiritXP;
						nextStateAfterSpiritXP = ems_Incremental;
					}
				}
				if (IncrementalSDL::saveRequestFlag)
				{
					IncrementalSDL::saveRequestFlag = false;
					SaveData();
				}

				if (IncrementalSDL::wantToLeave || externalinteg::wantLeave)
				{
					nextState = ems_Room;
					IncrementalSDL::wantToLeave = false;
					externalinteg::wantLeave = false;
					int fullXP = SpiritXPScreen::totalXP;
					if (fullXP > 0)
					{
						nextState = ems_SpiritXP;
						nextStateAfterSpiritXP = ems_Room;
					}
				}
				sdlloop = Loop_SDL();
				break;
			case ems_StoryCharge: {
				int aux = storyMessages.size() - 1;
				if (storyProgress >= aux)
				{
					//nextState = ems_Title;
					ResumeBattleOrGoToHub();
					storyProgressM = 0;
					ReverseScreenTransition();
					//Loop_SDL();

				}
				else {
					float initialProgressRatio = 0.2f;
					StoryUnlockedAndRecalculateStoryGoal();
					auto reqs = storyRequirements[storyProgress + 1];
					bool clearStoryReqs = true;
					clearStoryReqs = CheckRequirements(reqs);

					float previousTimeInStory = timeInStoryCharge;
					timeInStoryCharge += deltaTimeSDL;

					float progressTime = 0;
					const float minTime = 0.7f;
					const float timeCharge = 0.8f;

					float finalProgressRatio = 0.8f;
					initialProgressRatio = storyGoalP_Last / storyGoal;
					if (initialProgressRatio > 1) initialProgressRatio = 1;
					finalProgressRatio = storyGoalP / storyGoal;
					if (finalProgressRatio > 1) finalProgressRatio = 1;
					float progressRatioTime = 0;

					bool chargeNecessary = initialProgressRatio != finalProgressRatio;

					bool playSound = false;
					if (previousTimeInStory < minTime + timeCharge && chargeNecessary)
					{
						if (previousTimeInStory > minTime + timeCharge)
						{
							playSound = true;
						}
					}
					if (previousTimeInStory == 0 && chargeNecessary == false)
					{
						playSound = true;
					}


					if (timeInStoryCharge > minTime)
					{
						progressTime = timeInStoryCharge - minTime;
						progressRatioTime = progressTime / timeCharge;
						if (initialProgressRatio == finalProgressRatio)
						{
							progressRatioTime = 1.1f;
						}
					}

					if (progressRatioTime > 1)
					{
						progressRatioTime = 1;
						if (enterDownFrame)
						{




							int previousStoryWanted = -1;
							for (size_t i = 0; i <= storyProgress; i++)
							{
								if (!VectorUtils::VectorContains(storiesSeen, i))
								{
									previousStoryWanted = i;
									break;
									//previous story not seen yet
								}
							}
							if (previousStoryWanted >= storyMessages.size())
							{
								previousStoryWanted = -1;
							}

							if (finalProgressRatio >= 1 && clearStoryReqs)
							{
								storyGoalP = 0;
								storyProgress++;
								VariableGP::UpdateIfHigher(VariableGP::brisaVariables::statStoryProgress, storyProgress);
								nextState = ems_Choice;//#story #state


								BattleChoices::Reset();
								if (previousStoryWanted == -1)
								{
									BattleChoices::choices.push_back(BattleChoices::ChoicesE::StoryWatch);
									BattleChoices::choices.push_back(BattleChoices::ChoicesE::StoryLater);
									BattleChoices::choices.push_back(BattleChoices::ChoicesE::BackToHub);
								}
								else
								{
									BattleChoices::choices.push_back(BattleChoices::ChoicesE::StoryLater);
									BattleChoices::choices.push_back(BattleChoices::ChoicesE::StoryCatchup);
									BattleChoices::choices.push_back(BattleChoices::ChoicesE::BackToHub);
								}


								StopMusicSDL();
							}
							else
							{
								if (previousStoryWanted == -1)
								{
									ResumeBattleOrGoToHub();
								}
								else
								{
									BattleChoices::Reset();
									BattleChoices::choices.push_back(BattleChoices::ChoicesE::StoryLater);
									BattleChoices::choices.push_back(BattleChoices::ChoicesE::StoryCatchup);
									BattleChoices::choices.push_back(BattleChoices::ChoicesE::BackToHub);
									nextState = ems_Choice;
								}
								ReverseScreenTransition();


							}
							SaveData();
						}
					}

					float progress = progressRatioTime * (finalProgressRatio - initialProgressRatio) + initialProgressRatio;

					int xoff = 30;
					int y = 80;
					int h = 30;

					SDL_Color barColor = { 255, 255, 255, 255 };
					if (!clearStoryReqs || timeInStoryCharge < minTime)
					{
						barColor = color_darkcool;
					}
					bool success = progress == 1 && clearStoryReqs;
					if (success)
					{
						barColor = color_bright2;
						if (fmod(timeInStoryCharge, 0.4f) > 0.3f)
						{
							barColor = color_bright;
						}
					}

					if (playSound)
					{
						if (success)
						{
							PlaySound(SoundEnum::enemydead);
						}
						else
						{
							PlaySound(SoundEnum::enemydamage);
						}

					}

					AddRectangleS({ xoff, y, widthSDL - xoff * 2, h }, { 255,255,255,255 }, 6);
					AddRectangleS({ xoff + 2, y + 2, widthSDL - xoff * 2 - 2 * 2, h - 2 * 2 }, { 0,0,0,255 }, 6);

					AddRectangleS({ xoff + 4, y + 4, (int)((widthSDL - xoff * 2 - 4 * 2)*progress), h - 4 * 2 }, barColor, 6);
					DrawTextSDL(TextRender("Story progress", widthSDL / 2, 40, { 255,255,255,255 }, 2, 0, -1, -1));

					for (size_t i = 0; i < reqs.size(); i++)
					{
						DrawTextSDL(TextRender(reqs[i].description.c_str(), widthSDL / 2, 145 + i * 20, barColor, 2, 0, -1, -1));
					}
				}

				sdlloop = Loop_SDL();

			}
								  break;
			case ems_MainStory://#story
			{
				if (enterDownFrame)
				{
					storyProgressM++;

					if (storyProgressM >= storyMessages.at(storyCurrent).size())
					{
						//if stories need to be seen, go back to story selection
						int previousStoryWanted = -1;
						for (size_t i = 0; i < storyProgress; i++)
						{
							if (!VectorUtils::VectorContains(storiesSeen, i))
							{
								previousStoryWanted = i;
								break;
							}
						}
						if (previousStoryWanted >= 0)
						{
							BattleChoices::Reset();
							if (previousStoryWanted == -1)
							{
								BattleChoices::choices.push_back(BattleChoices::ChoicesE::StoryWatch);
								BattleChoices::choices.push_back(BattleChoices::ChoicesE::StoryLater);
								BattleChoices::choices.push_back(BattleChoices::ChoicesE::BackToHub);
							}
							else
							{
								BattleChoices::choices.push_back(BattleChoices::ChoicesE::StoryLater);
								BattleChoices::choices.push_back(BattleChoices::ChoicesE::StoryContinue);
								BattleChoices::choices.push_back(BattleChoices::ChoicesE::BackToHub);
							}
							nextState = ems_Choice;
						}
						else
						{
							//nextState = ems_Title;
							ResumeBattleOrGoToHub();

						}

						storyProgressM = 0;


						if (!VectorUtils::VectorContains(storiesSeen, storyCurrent))
						{
							storiesSeen.push_back(storyCurrent);
						}

						ReverseScreenTransition();
					}
					else {

					}
				}
				else
				{
					if (storyCurrent < storyMessages.size())
					{
						std::string m = storyMessages.at(storyCurrent).at(storyProgressM);
						StoryBrisa::RenderScene(m);

					}
				}

				sdlloop = Loop_SDL();
			}
			break;
			case ems_Cutscene:
			{
				sdlloop = Loop_SDL();
				StoryBrisa::Loop(enterDownFrame);
				if (StoryBrisa::GetExecutionData().external == "incrementalgo")
				{
					nextState = ems_Incremental;
				}
				if (StoryBrisa::GetExecutionData().external == "tutorialstart")
				{
					roomAlias = "tutorial";
					nextState = ems_Room;
				}
				if (StoryBrisa::GetExecutionData().external == "sceneend")
				{
					if (DungeonMap::Progress().processingSpot)
					{
						DungeonMap::Progress().currentSpotEvent++;
						nextState = ems_MapSpot;
					}
				}
				if (StoryBrisa::GetExecutionData().waitingForChoice)
				{
					nextState = ems_Choice;
				}
			}
			break;
			case ems_SpiritXP:
			{
				sdlloop = Loop_SDL();//SDL backend code
				int result = SpiritXPScreen::Draw(enterDownFrame);
				if (result < 0)
					nextState = nextStateAfterSpiritXP;

			}
			break;
			case ems_Gameover:
			{
				sdlloop = Loop_SDL();//SDL backend code
				int result = gameover::Update(deltaTimeSDL, enterDownFrame);

				if (enterPressed && renderScreenTransition == false && result == -1)
				{
					StartScreenTransition();
				}
				if (overScreenTransition && renderScreenTransition)
				{
					ReverseScreenTransition();
					//state = ems_Title;
					timeInStoryCharge = 0;
					nextState = ems_StoryCharge;

				}

			}
			break;
			default:
				break;
			}


			//state transition code! #statechange #statetransition
			if (state != nextState)
			{
				positionOffsetSDL.Set(0, 0);
				Dialog::CloseAll();
				timeOnState = 0;
				if (drawDebug)
				{
					SDL_Log("State change");
				}
				if (nextState == MainState::ems_Incremental)
				{
					if (!VectorUtils::VectorContains(persData.cutscenesSeen, "miguelbefore"))
					{
						StoryBrisa::StartCutscene("miguelbefore");
						nextState = ems_Cutscene;
					}
					else {
						IncrementalSDL::Enter();
					}

				}
				if (nextState == MainState::ems_Gameover)
				{
					gameover::Enter();
				}
				if (nextState == MainState::ems_Choice)
				{

					BattleLoop_BasicInit();
					BattleLoop_BattleInit();
					BattleLoop_Peace();
					BattleChoices::FeedChoices();
				}
				if (nextState == MainState::ems_SpiritXP)
				{
					if (powerup::activeSpirits.size() <= 0)
					{
						nextState = nextStateAfterSpiritXP;
					}
					SpiritXPScreen::Reset();
				}
				if (nextState == MainState::ems_Room)
				{
					bool noTuto = VariableGP::variables[VariableGP::brisaVariables::tutoPortal] == 0 && roomAlias != "tutorial";
					bool spiritWantToShow = powerup::spiritWantToShow >= 0;
					if (noTuto && spiritWantToShow == false)
					{

						//roomAlias == "tutorial";
						StoryBrisa::StartCutscene("sarabefore");
						nextState = ems_Cutscene;
					}
					else
					{
						PlaySound(SoundEnum::enterplace);

						reloadRoomState = true;
					}
					StatLeader::ReportData();
				}
				if (nextState == MainState::ems_Battle)
				{
					BattleLoop_BasicInit();
					BattleLoop_BattleInit();

					PlayMusic(MusicEnum::battle);
					if (hasBattleData)
					{
						if (currentSaveVersion < 0) currentSaveVersion = latestSaveVersion;
						BattleLoop_LoadData(loadedBattleData, currentSaveVersion);
					}
					if (DungeonMap::CurrentMap().spots.size() > 0)
					{
						bd.goingThroughDungeonMap = true;

						int bat = DungeonMap::currentBattleGen();
						auto bg = DungeonMap::CurrentMap().battleGens[bat];
						bd.groupEnemySpawn = bg.groupEnemySpawn;
						bd.maxWave = RandomRangeInt(bg.waveNRange.x, bg.waveNRange.y);
						bd.wave = -1;

						{
							BattleWavePrizeChoice bwpc;
							{
								BattleWavePrizeChoiceOption bwpco;
								bwpco.battlePrize.crystalN = 15;
								bwpco.optionName = "Go South";
								bwpco.optionDescription = "Feels like there might be experience crystals";
								bwpc.options.push_back(bwpco);
							}
							{
								BattleWavePrizeChoiceOption bwpco;
								bwpco.battlePrize.resourceId = IncrementalBrisa::resolveIds("res:leaf");
								bwpco.battlePrize.resource = 15;
								bwpco.optionName = "Go North";
								bwpco.optionDescription = "You see a hint of leafs";
								bwpc.options.push_back(bwpco);
							}

							bd.battleWavePrizeOptions.push_back(bwpc);
						}


						/*bd.battlePrize.crystalN = 5;
						bd.battlePrize.resourceId = IncrementalBrisa::resolveIds("res:leaf");
						bd.battlePrize.resource = 5;*/
					}
					battleStartTime = currentTimeSDL;
				}
				if (nextState == MainState::ems_MainStory)
				{
					int previousStoryWanted = -1;
					for (size_t i = 0; i < storyProgress; i++)
					{
						if (!VectorUtils::VectorContains(storiesSeen, i))
						{
							previousStoryWanted = i;
							break;
						}
					}
					if (previousStoryWanted == -1)
					{
						storyCurrent = storyProgress;
					}
					else
					{
						storyCurrent = previousStoryWanted;
					}
				}


				if (state == MainState::ems_Battle)
				{
					StopMusicSDL();
				}
				if (state == MainState::ems_Choice)
				{
					BattleChoices::Reset();
				}
				state = nextState;
			}

			if (reloadRoomState)
			{
				Dialog::CloseAll();
				BattleLoop_BasicInit();
				BattleLoop_BattleInit();
				BattleLoop_Peace();
				BattleLoop_RoomMode();
				RoomCode::Room r;
				r.mapId = 0;
				for (size_t i = 0; i < MapRendering::aliases.size(); i++)
				{
					if (MapRendering::aliases[i] == roomAlias)
					{
						r.mapId = i;
					}
				}
				if (roomAlias == "hub") {
					r.walkOnNothing = false;
				}
				else
				{
					r.walkOnNothing = true;
				}
				RoomCode::EnterRoom(r);
				BattleLoop_HeroCamera(true);

				std::vector<std::string> simpleNPCPositions;
				std::vector<int> simpleNPCDialogId;
				std::vector<int> simpleNPCmetaOrObject;


				if (roomAlias == "tutorial")
				{


					auto npc1pos = RoomCode::GetMetaPositionWorld("NPC1");
					{
						Entity e;
						action::Action a;
						a.type = action::Type::dialog;
						a.intOnly = 0;
						e.action.actions.pc_actions.push_back(a);
						e.action.actions.s_actions.push_back(a);
						{
							{
								action::Action a;
								a.type = action::Type::TeleportHero;
								a.positionOnly.posTypeX = action::PositionType::Hero;
								a.positionOnly.posTypeY = action::PositionType::Self;
								a.positionOnly.source.Set(0, 25);
								a.positionOnly.offset.Set(0, 0);
								a.boolD = true;
								e.action.actions.pc_actions.push_back(a);
							}
							{
								action::Action a;
								a.type = action::Type::variableChange;
								a.intArray[0] = VariableGP::brisaVariables::tutoPortal;
								a.intArray[1] = 1;
								Requirement r;
								r.type = RequirementTypes::dialogabove;
								r.n1 = 0;
								r.n2 = 1;
								a.reqs.push_back(r);
								e.action.actions.pc_actions.push_back(a);
							}
							{
								action::Action a;
								a.type = action::Type::Nothing;
								a.intOnly = 0;
								a.time = 2;
								e.action.actions.actions.push_back(a);
							}


							//e.action.actions.pc_actions.push_back(a);


						}

						e.position = npc1pos;
						e.type = 3;
						e.size.Set(16, 16);
						bd.ents[3].push_back(e);
					}
				}

				deadenemypositionsForRoom.clear();
				if (roomAlias == "hub")
				{
					simpleNPCPositions.push_back("NPC1");
					simpleNPCPositions.push_back("NPC2");
					simpleNPCPositions.push_back("NPC3");
					simpleNPCPositions.push_back("NPC4");
					simpleNPCPositions.push_back("npc_sanct");
					simpleNPCDialogId.push_back(2);
					simpleNPCDialogId.push_back(1);
					simpleNPCDialogId.push_back(3);
					simpleNPCDialogId.push_back(4);
					simpleNPCDialogId.push_back(5);
					simpleNPCmetaOrObject = { 0,0,0,0,1 };

					for (size_t i = 0; i < 200; i++)
					{
						auto v = Vector2D(RandomRangeInt(0, 90), RandomRangeInt(0, heightSDL));
						if (RandomRange(0, 1) > 0.5f)
						{
							v.x = widthSDL - v.x;
						}
						deadenemypositionsForRoom.push_back(v);
					}
				}

				for (size_t i = 0; i < simpleNPCDialogId.size(); i++)
				{
					Vector2D npc1pos;
					if (simpleNPCmetaOrObject[i] == 0)
						npc1pos = RoomCode::GetMetaPositionWorld(simpleNPCPositions[i]);
					else
						npc1pos = RoomCode::GetObjectPositionWorld(simpleNPCPositions[i]);
					Entity e;
					action::Action a;
					a.type = action::Type::dialog;
					a.intOnly = simpleNPCDialogId[i];
					e.action.actions.pc_actions.push_back(a);
					//e.action.actions.s_actions.push_back(a);
					{
						{
							action::Action a;
							a.type = action::Type::TeleportHero;
							a.positionOnly.posTypeX = action::PositionType::Hero;
							a.positionOnly.posTypeY = action::PositionType::Self;
							a.positionOnly.source.Set(0, 25);
							a.positionOnly.offset.Set(0, 0);
							a.boolD = true;
							e.action.actions.pc_actions.push_back(a);
						}
						{
							action::Action a;
							a.type = action::Type::variableChange;
							a.intArray[0] = VariableGP::brisaVariables::tutoPortal;
							a.intArray[1] = 1;
							Requirement r;
							r.type = RequirementTypes::dialogabove;
							r.n1 = 0;
							r.n2 = 1;
							a.reqs.push_back(r);
							e.action.actions.pc_actions.push_back(a);
						}
						{
							action::Action a;
							a.type = action::Type::Nothing;
							a.intOnly = 0;
							a.time = 2;
							e.action.actions.actions.push_back(a);
						}


						//e.action.actions.pc_actions.push_back(a);


					}

					e.position = npc1pos;
					e.type = 3;
					e.size.Set(16, 16);
					bd.ents[3].push_back(e);
				}

			}



		}

		if (sdlloop < 0)//end game code　ゲームの終わり条件
		{
			gameRunning = false;
			//change this to return, make function to free stuff and shit?
			return;//end game
		}
	}

	ColoredRects::Update();
	ColoredRects::Render();



	Profiler::EndMaster();



	Profiler::enabled = drawDebug;
	if (drawDebug)
	{
		Profiler::StartTime("profiling");
		Profiler::Update();
		Profiler::Draw();

		if (Profiler::debugCommandInput)
		{
			if (Profiler::debugCommand == "reset")
			{
				persistence::Reset(saveFilename);
				LoadDataFromFiles();
			}
			if (Profiler::debugCommand == "savespecial")
			{
				auto fulldata = FileUtils::ReadFromFile("savedata/savedata.xml");
				LoadMasterDataString(fulldata);
			}
			if (Profiler::debugCommand == "importsave")
			{
				auto fulldata = FileUtils::ReadFromFile("savedata/export.xml");
				LoadMasterDataString(fulldata);
			}
			if (Profiler::debugCommand == "exportsave")
			{
				FileUtils::WriteToFile("savedata/export.xml", BrowserSave::lastSave);
			}
			if (Profiler::debugCommand == "saveconsistency")
			{
				auto str = persistence::SaveDataAsString();
				LoadDataFromString(str);
				auto str2 = persistence::SaveDataAsString();
				if (str != str2)
				{
					SDL_Log("SAVE INCONSISTENT");
				}
				else
				{
					SDL_Log("SAVE CONSISTENT");
				}
				//persistence::LoadStartFromString(str);
			}

			if (Profiler::debugCommand == "choice")
			{
				nextState = MainState::ems_Choice;
			}

			if (Profiler::debugCommand == "timeshow")
			{
				SDL_Log("Days: %d", daysSDL);
				SDL_Log("Seconds: %d", secondsSDL);
			}
			if (Profiler::debugCommand == "joinspirit")
			{
				powerup::SpiritJoin(0);
			}
			if (Profiler::debugCommand == "joinspiritall")
			{
				powerup::joinedSpirits.clear();
				for (size_t i = 0; i < 15; i++)
				{

					int id = powerup::SpiritJoin(14);
					powerup::joinedSpirits[id].level = 12;

				}


			}
			if (Profiler::debugCommand == "deltaplus")
			{
				deltaMultiplier = 10;
			}
			if (Profiler::debugCommand == "deltahuge")
			{
				deltaMultiplier = 1000;
			}
			if (Profiler::debugCommand == "deltazerothree")
			{
				deltaOverwrite = 0.3f;
			}
			if (Profiler::debugCommand == "savetest")
			{
				latestSaveVersion--;
				SaveData();
				latestSaveVersion++;
				LoadDataFromFiles();
			}
			if (Profiler::debugCommand == "stst1")
			{
				//persistence::Init("/testd");
				persistence::SaveStart("savedata/test.bin");
				persistence::WriteInt(1);
				persistence::SaveEnd();
				persistence::Commit();
				SDL_Log("save test a");
			}
			if (Profiler::debugCommand == "stst2")
			{
				//persistence::Init("/testd");
				persistence::SaveStart("savedata/test.bin");
				persistence::WriteInt(2);
				persistence::SaveEnd();
				persistence::Commit();
				SDL_Log("save test b");
			}
			if (Profiler::debugCommand == "stst3")
			{
				//persistence::Init("/testd");
				persistence::LoadStart("savedata/test.bin");
				int i = persistence::ReadInt();
				SDL_Log("Loaded test: %d", i);
				persistence::LoadEnd("savedata/test.bin");
			}
			if (Profiler::debugCommand == "stst4")
			{
				//persistence::Init("/testd");
				persistence::LoadStart("savedata/test.bin");
				int i = persistence::ReadInt();
				SDL_Log("Loaded test: %d", i);
				persistence::LoadEnd("savedata/test.bin");

				persistence::SaveStart("savedata/test.bin");
				persistence::WriteInt(i);
				persistence::SaveEnd();
				persistence::Commit();


			}
			if (Profiler::debugCommand == "resetincr")
			{
				FileUtils::WriteToFile(IncrementalSDL::filename, "");
				IncrementalSDL::LoadData(currentSaveVersion);
			}
			if (Profiler::debugCommand == "spiritxp")
			{

				nextState = MainState::ems_SpiritXP;
				BattleLoop_BankedXP() += 200;
				SpiritXPScreen::totalXP += 200;
			}
			if (Profiler::debugCommand == "spiritxp2")
			{

				nextState = MainState::ems_SpiritXP;
				BattleLoop_BankedXP() += 2000;
				SpiritXPScreen::totalXP += 2000;
			}
			if (Profiler::debugCommand == "sanctuary")
			{

				for (size_t i = 0; i < farm::spiritsInFarm.size(); i++)
				{
					powerup::joinedSpirits[farm::spiritsInFarm[i]].farmxp += 100000;
				}
			}
			if (Profiler::debugCommand == "storygoa")
			{
				storyGoalP += 500;
			}
			if (Profiler::debugCommand == "storygob")
			{
				float timeOnBattle = currentTimeSDL - battleStartTime;
				storyGoalP = storyGoal - 5 - timeOnBattle;
			}
			if (Profiler::debugCommand == "storylast")
			{
				storyProgress = storyMessages.size() - 2;
			}
			if (Profiler::debugCommand == "particlee")
			{
				ParticleExecuter::particleEnabled = !ParticleExecuter::particleEnabled;
			}
			if (Profiler::debugCommand == "storyprint")
			{
				std::string script;
				for (size_t i = 0; i < storyMessages.size(); i++)
				{
					auto scene = storyMessages[i];
					script.append("Scene ");
					script.push_back((char)'1' + i);
					script.push_back('\n');
					script.push_back('\n');
					script.append("===================================");
					script.push_back('\n');
					for (size_t j = 0; j < scene.size(); j++)
					{
						auto m = scene[j];
						script.append(m);
						script.push_back('\n');
						script.push_back('\n');

					}
					script.push_back('\n');
					script.append("===================================");
					script.push_back('\n');
				}
				//SDL_Log(script.c_str());
				FileUtils::WriteToFile("storyscript.txt", script);
			}

		}
		Profiler::EndTime("profiling");
	}

}

bool CheckRequirements(std::vector<Requirement> reqs)
{
	for (size_t i = 0; i < reqs.size(); i++)
	{
		if (reqs[i].type == RequirementTypes::enemy_Defeated)
		{
			int enemyId = reqs[i].n1;
			int enemyAm = reqs[i].n2;
			if (enemyDefeatedAmount_pers[enemyId] < enemyAm)
			{
				return false;
			}
		}
		if (reqs[i].type == RequirementTypes::story)
		{
			int storyPReq = reqs[i].n1;
			if (storyPReq != storyProgress)
			{
				return false;
			}
		}
		if (reqs[i].type == RequirementTypes::storyover)
		{
			int storyPReq = reqs[i].n1;
			if (storyPReq >= storyProgress)
			{
				return false;
			}
		}
	}

	return true;
}

bool StoryUnlockedAndRecalculateStoryGoal()
{
	if (storyRequirements.size() <= storyProgress + 1) return false;
	auto timeOnBattle = 0;
	if (state == ems_Battle)
	{
		timeOnBattle = currentTimeSDL - battleStartTime;
	}
	storyGoal = storyProgress * 3.0f / storyMessages.size();
	storyGoal += 2;
	storyGoal *= 60; //from minutes to second
	auto reqs = storyRequirements[storyProgress + 1];
	if (reqs.size() > 0)
	{
		storyGoal = 1;
	}

	if (storyGoalP + timeOnBattle < storyGoal) return false;

	bool clearStoryReqs = true;
	clearStoryReqs = CheckRequirements(reqs);
	if (!clearStoryReqs) return false;
	return true;
}

