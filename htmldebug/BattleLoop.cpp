#include "BattleLoop.h"
#include "EnemyFollow.h"
#include "BattleDataReader.h"
#include "Sound.h"
#include "Main.h"
#include "Profiler.h"
#include "AtlasManager.h"
#include "BattleChoice.h"
#include "Dialog.h"
#include "VariableGP.h"
#include <array>
#include "TransformAnimation.h"


const int groupEnemySpawnSize = 12;
const int mx = 9999999;

BattleData bd;

int playerSoundMiss = 2, playerSoundGood = 3, enemySoundRandomMove = 1, enemySoundAttack = 4;

std::vector<int> enemyDefeatedAmount_pers;
Vector2D dir;
int movementMode = 0;
bool renderChoices = false;
int heroMinDisToDamage = 400;
float deltaTimeSDLOriginal;
int baseDamage = 5;
int enemyTutorialY = 100;
int xpReq;


namespace powerup
{
	std::vector<uisystem::ConfirmableButton> buttons;
	std::vector<PowerUp> powerUpDatabase;
	std::vector<PowerUp> permanentPowerUps;
	std::vector<BattleModifierMod> permanentPowerUpMods;
	std::vector<Spirit> joinedSpirits;
	std::vector<int> activeSpirits;

	std::vector<int> enemyIdToSpirit;

	int maxActiveSpirit = 20;
	int maxJoinableSpiritAmount = 20;
	int maxJoinableSpiritAmountCeiling = 50; //used while savingdata
	int maxActiveSpiritCeiling = 20; //used while saving data

	bool doesHeroModifierTrigger(int bm)
	{
		float p = bd.heroModifiers[bm].prob;
		if (p >= 1) return true;
		return RandomRange(0, 1) < p;
	}



	int SpiritJoin(int enemyId)
	{
		powerup::Spirit sp;
		sp.enemyId = enemyId;
		int rarity = enemyIdToSpirit[enemyId];

		vector<int> possiblePus;
		for (size_t i = 0; i < powerUpDatabase.size(); i++)
		{
			if (powerUpDatabase[i].spiritRarity == rarity)
			{
				possiblePus.push_back(i);
			}
		}
		if (possiblePus.size() == 0)
		{
			SDL_Log("Problem with joined spirit!");
		}
		sp.powerUpIds[0] = possiblePus[RandomRangeInt(0, possiblePus.size())];

		for (size_t i = 0; i < powerup::joinedSpirits.size(); i++)
		{
			if (powerup::joinedSpirits[i].enemyId < 0)
			{
				powerup::joinedSpirits[i] = sp;
				return i;
				break;
			}

		}
		powerup::joinedSpirits.push_back(sp);
		return powerup::joinedSpirits.size() - 1;
	}

	int SpiritJoin(int enemyId, float battlePowerB, float battlePowerLevel)
	{
		int ind = SpiritJoin(enemyId);
		joinedSpirits[ind].battlePowerBase = battlePowerB;
		joinedSpirits[ind].battlePowerPerLevel = battlePowerLevel;
		return ind;
	}

	float CalculateAmount(int bm, int defaultValue)
	{
		if (doesHeroModifierTrigger(bm))
		{
			return bd.heroModifiers[bm].amount + bd.heroModifiersMods[bm].addValue;
		}
		else
		{
			return defaultValue;
		}
	}

	void ModifyPowerUp(PowerUp pu, int spiritRarity, float prob, float effectMultiplier = 1.0f)
	{
		for (size_t i = 0; i < powerUpMaxMod; i++)
		{
			pu.mod[i].prob = prob;
			pu.mod[i].amount *= effectMultiplier;
		}
		pu.spiritRarity = spiritRarity;
		int probI = prob * 100;

		powerUpDatabase.push_back(pu);
	}



	int EquipSpirit(int obtainedId)
	{
		Spirit sp = joinedSpirits[obtainedId];
		for (size_t i = 0; i < activeSpirits.size(); i++)
		{
			if (sp.enemyId == joinedSpirits[activeSpirits[i]].enemyId)
			{
				activeSpirits[i] = obtainedId; //swaps spirit
				return 1;
			}
		}
		if (activeSpirits.size() < maxActiveSpirit)
		{
			activeSpirits.push_back(obtainedId);
			return 0;
		}
		else
		{
			return -1;
		}

	}

	bool desperateForSpiritsJoin()
	{
		int spiritsJoinedN = 0;
		for (size_t i = 0; i < powerup::joinedSpirits.size(); i++)
		{
			if (powerup::joinedSpirits[i].enemyId >= 0)
			{
				spiritsJoinedN++;
			}
		}
		return spiritsJoinedN < 3;
	}

	bool CanASpiritJoin()
	{
		int spiritsJoinedN = 0;
		for (size_t i = 0; i < powerup::joinedSpirits.size(); i++)
		{
			if (powerup::joinedSpirits[i].enemyId >= 0)
			{
				spiritsJoinedN++;
			}
		}
		return maxJoinableSpiritAmount > spiritsJoinedN;
	}

	std::string PowerUpName(Spirit sp)
	{
		return powerUpDatabase[sp.powerUpIds[0]].name;
	}

	PowerUp PowerUpMain(Spirit sp)
	{
		return powerUpDatabase[sp.powerUpIds[0]];
	}

	int XPForLevel(Spirit sp)
	{
		return sp.level * 30 + sp.level * sp.level * 10 + 20;
	}

	BattleModifierMod CalculateModifier(PowerUp pup, int level)
	{
		auto bmm = BattleModifierMod();
		bmm.addProbability = level * 0.05f;
		bmm.addValue = (level / 5)*pup.mod[0].amount;
		return bmm;
	}

	void Init()
	{
		powerUpDatabase.clear();
		std::string longUnlock = "longunlock";
		std::string closeUnlock = "closeunlock";



		//power up database feed
		//always add at the end of the list to not break compatibility
		{
			{
				{ //0
					PowerUp pu;
					pu.mod[0].amount = 3; pu.mod[0].type = BattleModifierType::damageAdd;
					pu.mod[0].range = PlayerAttackRange::medium;
					pu.mod[0].move = PlayerAttackMovement::any;
					pu.name = "Attack Power+";
					pu.description = "Enhances the power of your basic attack";
					powerUpDatabase.push_back(pu);
				}
				{ //1
					PowerUp pu;
					pu.mod[0].amount = 0.6f; pu.mod[0].type = BattleModifierType::bulletTimeCDMultiplier;
					pu.mod[1].amount = 1.5f; pu.mod[1].type = BattleModifierType::bulletTimeTimeMultiplier;
					pu.name = "Bullet Time+";
					pu.description = "Reduce cooldown and make bullet time last longer";
					powerUpDatabase.push_back(pu);
				}
				{//2
					PowerUp pu;
					pu.mod[0].amount = 15.0f; pu.mod[0].type = BattleModifierType::damageAdd;
					pu.mod[1].amount = 1.5f; pu.mod[1].type = BattleModifierType::rangeMultiplier;
					pu.mod[0].move = PlayerAttackMovement::any;
					pu.mod[0].range = PlayerAttackRange::any;
					pu.mod[1].move = PlayerAttackMovement::any;
					pu.mod[1].range = PlayerAttackRange::any;
					pu.mod[0].danger = GenericCondition::TRUE;
					pu.mod[1].danger = GenericCondition::TRUE;
					pu.name = "Danger Power Up";
					pu.description = "Increase attack power and range when near an opponent's attack";
					powerUpDatabase.push_back(pu);
				}
				{//3
					PowerUp pu;
					pu.mod[0].amount = 1.5f; pu.mod[0].type = BattleModifierType::rangeMultiplier;
					pu.mod[0].range = PlayerAttackRange::medium;
					pu.mod[0].move = PlayerAttackMovement::any;
					pu.mod[1].amount = 1.0f; pu.mod[1].type = BattleModifierType::damageAdd;
					pu.mod[1].range = PlayerAttackRange::medium;
					pu.mod[1].move = PlayerAttackMovement::any;
					pu.name = "Attack Range+";
					pu.description = "Enhance reach and a bit of the power of your basic attack";
					powerUpDatabase.push_back(pu);
				}
				{//4
					PowerUp pu;
					pu.mod[0].amount = 3.0f; pu.mod[0].type = BattleModifierType::lifeIncrease;
					pu.name = "Dance Group+";
					pu.description = "Increases the number of enemies who can dance with you by 3";
					powerUpDatabase.push_back(pu);
				}
				{//5
					PowerUp pu;
					pu.mod[0].amount = 3.0f; pu.mod[0].type = BattleModifierType::combolength;
					pu.name = "Attack Combo";
					pu.description = "Allows you to do a combo when standing still";
					powerUpDatabase.push_back(pu);
				}
				{//6
					PowerUp pu;
					pu.mod[0].amount = 2.0f; pu.mod[0].type = BattleModifierType::damageAdd;
					pu.mod[0].range = PlayerAttackRange::any;
					pu.mod[0].move = PlayerAttackMovement::still;
					pu.name = "Contained Dancing";
					pu.description = "Increases attack power when you are not moving";
					powerUpDatabase.push_back(pu);
				}
				{//7
					PowerUp pu;
					pu.mod[0].amount = 1.0f; pu.mod[0].type = BattleModifierType::damageAdd;
					pu.mod[0].range = PlayerAttackRange::any;
					pu.mod[0].move = PlayerAttackMovement::moving;
					pu.name = "Relentless Attacker";
					pu.description = "Increases attack power when you are moving";
					powerUpDatabase.push_back(pu);
				}
				{//8
					PowerUp pu;
					pu.mod[0].type = BattleModifierType::attackUnlock;
					pu.mod[0].range = PlayerAttackRange::close;
					pu.mod[0].move = PlayerAttackMovement::any;
					pu.tag = closeUnlock;
					pu.forbiddenTags.push_back(pu.tag);
					pu.name = "Close-range attack";
					pu.description = "Adds a close-range attack to your basic attack";

					powerUpDatabase.push_back(pu);
				}

				{//9
					PowerUp pu;
					pu.mod[0].amount = 4.0f; pu.mod[0].type = BattleModifierType::damageAdd;
					pu.mod[0].range = PlayerAttackRange::close;
					pu.mod[0].move = PlayerAttackMovement::any;
					pu.name = "Attack Power+ Close";
					pu.description = "Enhances the power of Close-range attacks";
					pu.necessaryTags.push_back(closeUnlock);
					powerUpDatabase.push_back(pu);
				}
				{//10
					PowerUp pu;
					pu.mod[0].amount = 1.5f; pu.mod[0].type = BattleModifierType::rangeMultiplier;
					pu.mod[0].range = PlayerAttackRange::close;
					pu.mod[0].move = PlayerAttackMovement::any;
					pu.mod[1].amount = 1.0f; pu.mod[1].type = BattleModifierType::damageAdd;
					pu.mod[1].range = PlayerAttackRange::close;
					pu.mod[1].move = PlayerAttackMovement::any;
					pu.necessaryTags.push_back(closeUnlock);
					pu.name = "Attack Range+ Close";
					pu.description = "Enhance reach and a bit of the power of close-range attacks";
					powerUpDatabase.push_back(pu);
				}


				{//11
					PowerUp pu;
					pu.mod[0].type = BattleModifierType::attackUnlock;
					pu.mod[0].range = PlayerAttackRange::far;
					pu.mod[0].move = PlayerAttackMovement::any;
					pu.name = "Long-range Attack";
					pu.description = "Adds a long-range attack to your basic attack";
					pu.tag = longUnlock;
					pu.forbiddenTags.push_back(pu.tag);
					powerUpDatabase.push_back(pu);
				}
				{//12
					PowerUp pu;
					pu.mod[0].amount = 2.0f; pu.mod[0].type = BattleModifierType::damageAdd;
					pu.mod[0].range = PlayerAttackRange::far;
					pu.mod[0].move = PlayerAttackMovement::any;
					pu.name = "Attack Power+ Long";
					pu.description = "Enhances the power of long-range attacks";
					pu.necessaryTags.push_back(longUnlock);
					powerUpDatabase.push_back(pu);
				}
				{//13
					PowerUp pu;
					pu.mod[0].amount = 1.5f; pu.mod[0].type = BattleModifierType::rangeMultiplier;
					pu.mod[0].range = PlayerAttackRange::far;
					pu.mod[0].move = PlayerAttackMovement::any;
					pu.mod[1].amount = 1.0f; pu.mod[1].type = BattleModifierType::damageAdd;
					pu.mod[1].range = PlayerAttackRange::far;
					pu.mod[1].move = PlayerAttackMovement::any;
					pu.name = "Attack Range+ Long";
					pu.description = "Enhance reach and a bit of the power of long-range attacks";
					pu.necessaryTags.push_back(longUnlock);
					powerUpDatabase.push_back(pu);
				}
			}
			{
				float prob = 0.2f;
				int spiritRarity = 0;
				powerup::ModifyPowerUp(powerUpDatabase[0], spiritRarity, prob);
				powerup::ModifyPowerUp(powerUpDatabase[2], spiritRarity, prob);
				powerup::ModifyPowerUp(powerUpDatabase[3], spiritRarity, prob);
				powerup::ModifyPowerUp(powerUpDatabase[6], spiritRarity, prob);
				powerup::ModifyPowerUp(powerUpDatabase[7], spiritRarity, prob);
				powerup::ModifyPowerUp(powerUpDatabase[9], spiritRarity, prob);
				powerup::ModifyPowerUp(powerUpDatabase[10], spiritRarity, prob);
				powerup::ModifyPowerUp(powerUpDatabase[12], spiritRarity, prob);
				powerup::ModifyPowerUp(powerUpDatabase[13], spiritRarity, prob);
			}
			{
				float prob = 0.5f;
				int spiritRarity = 1;
				powerup::ModifyPowerUp(powerUpDatabase[0], spiritRarity, prob);
				powerup::ModifyPowerUp(powerUpDatabase[2], spiritRarity, prob);
				powerup::ModifyPowerUp(powerUpDatabase[3], spiritRarity, prob);
				powerup::ModifyPowerUp(powerUpDatabase[6], spiritRarity, prob);
				powerup::ModifyPowerUp(powerUpDatabase[7], spiritRarity, prob);
				powerup::ModifyPowerUp(powerUpDatabase[9], spiritRarity, prob);
				powerup::ModifyPowerUp(powerUpDatabase[10], spiritRarity, prob);
				powerup::ModifyPowerUp(powerUpDatabase[12], spiritRarity, prob);
				powerup::ModifyPowerUp(powerUpDatabase[13], spiritRarity, prob);
			}
			{
				float prob = 0.5f;
				int spiritRarity = 2;
				float effectMultiplier = 2;
				powerup::ModifyPowerUp(powerUpDatabase[0], spiritRarity, prob, effectMultiplier);
				powerup::ModifyPowerUp(powerUpDatabase[2], spiritRarity, prob, effectMultiplier);
				powerup::ModifyPowerUp(powerUpDatabase[3], spiritRarity, prob, effectMultiplier);
				powerup::ModifyPowerUp(powerUpDatabase[6], spiritRarity, prob, effectMultiplier);
				powerup::ModifyPowerUp(powerUpDatabase[7], spiritRarity, prob, effectMultiplier);
				powerup::ModifyPowerUp(powerUpDatabase[9], spiritRarity, prob, effectMultiplier);
				powerup::ModifyPowerUp(powerUpDatabase[10], spiritRarity, prob, effectMultiplier);
				powerup::ModifyPowerUp(powerUpDatabase[12], spiritRarity, prob, effectMultiplier);
				powerup::ModifyPowerUp(powerUpDatabase[13], spiritRarity, prob, effectMultiplier);
			}
			{
				float prob = 0.7f;
				int spiritRarity = 3;
				float effectMultiplier = 2.5f;
				powerup::ModifyPowerUp(powerUpDatabase[0], spiritRarity, prob, effectMultiplier);
				powerup::ModifyPowerUp(powerUpDatabase[2], spiritRarity, prob, effectMultiplier);
				powerup::ModifyPowerUp(powerUpDatabase[3], spiritRarity, prob, effectMultiplier);
				powerup::ModifyPowerUp(powerUpDatabase[6], spiritRarity, prob, effectMultiplier);
				powerup::ModifyPowerUp(powerUpDatabase[7], spiritRarity, prob, effectMultiplier);
				powerup::ModifyPowerUp(powerUpDatabase[9], spiritRarity, prob, effectMultiplier);
				powerup::ModifyPowerUp(powerUpDatabase[10], spiritRarity, prob, effectMultiplier);
				powerup::ModifyPowerUp(powerUpDatabase[12], spiritRarity, prob, effectMultiplier);
				powerup::ModifyPowerUp(powerUpDatabase[13], spiritRarity, prob, effectMultiplier);
			}
			{
				float prob = 0.7f;
				int spiritRarity = 4;
				float effectMultiplier = 3.0f;
				powerup::ModifyPowerUp(powerUpDatabase[0], spiritRarity, prob, effectMultiplier);
				powerup::ModifyPowerUp(powerUpDatabase[2], spiritRarity, prob, effectMultiplier);
				powerup::ModifyPowerUp(powerUpDatabase[3], spiritRarity, prob, effectMultiplier);
				powerup::ModifyPowerUp(powerUpDatabase[6], spiritRarity, prob, effectMultiplier);
				powerup::ModifyPowerUp(powerUpDatabase[7], spiritRarity, prob, effectMultiplier);
				powerup::ModifyPowerUp(powerUpDatabase[9], spiritRarity, prob, effectMultiplier);
				powerup::ModifyPowerUp(powerUpDatabase[10], spiritRarity, prob, effectMultiplier);
				powerup::ModifyPowerUp(powerUpDatabase[12], spiritRarity, prob, effectMultiplier);
				powerup::ModifyPowerUp(powerUpDatabase[13], spiritRarity, prob, effectMultiplier);
			}

		}


	}

	void RecalculatePermanentPowerUps()
	{
		permanentPowerUpMods.clear();
		permanentPowerUps.clear();
		for (size_t i = 0; i < activeSpirits.size(); i++)
		{
			auto pups = &joinedSpirits[activeSpirits[i]].powerUpIds;
			int pu = pups->at(0);
			if (pu >= 0)
			{
				int level = joinedSpirits[activeSpirits[i]].level;
				auto pup = powerUpDatabase[pu];
				permanentPowerUps.push_back(pup);
				auto bmm = CalculateModifier(pup, level);
				permanentPowerUpMods.push_back(bmm);
			}
		}
	}
}

namespace BattleEffects
{
	enum class FixedEffects {
		PlayerDanceA, PlayerDanceB, playerDanceC,
	};
	enum class DatabaseEffects {
		//default type
		damageA, musicNote, damageB, skillActivate, AttackTell,
		Slash,

		//special type
		musicNoteXP,
	};

	int AddEffect(SpriteSheetEffect sse)
	{
		bool added = false;
		for (size_t i = 0; i < bd.ssEffect.size(); i++)
		{

			if (bd.ssEffect[i].inactiveEnd)
			{
				if (bd.ssEffect[i].progress >= bd.ssEffect[i].regions.size())
				{
					bd.ssEffect[i].active = false;
				}
			}
			if (bd.ssEffect[i].active == false)
			{
				bd.ssEffect[i] = sse;
				added = true;
				return i;
			}
		}
		if (added == false)
		{
			bd.ssEffect.push_back(sse);
			return bd.ssEffect.size() - 1;
		}
		return -1;
	}

}

//EnemySummonData enemySummonDatas[4] =
//{
//	{ 3, 0,  8},
//	{16, 1, 18},
//	{12, 2, 12},
//	{20, 3, 25},
//};

bool CheckBattleRequirement(std::vector<Requirement> reqs, Entity &owner)
{
	for (size_t i = 0; i < reqs.size(); i++)
	{
		if (reqs[i].type == RequirementTypes::dialogabove)
		{
			if (Dialog::dialogDatabase[reqs[i].n1].progress <= reqs[i].n2)
			{
				return false;
			}

		}

		auto hpper = owner.hp * 100;
		if (owner.maxHP > 0) hpper /= owner.maxHP;
		if (reqs[i].type == RequirementTypes::hpbelow)
		{
			if (owner.hp > (reqs[i].n1) * enemyHPMultiply) return false;
		}
		if (reqs[i].type == RequirementTypes::hpbelowpercent)
		{
			if (hpper > (reqs[i].n1)) return false;
		}
		if (reqs[i].type == RequirementTypes::hpabovepercent)
		{
			if (hpper <= (reqs[i].n1)) return false;
		}
	}
	return true;
}

void ChangeState(BattleState bs)
{
	bd.state = bs;
	bd.timeOnState = 0;
}

bool heroModifierMatchAttack(int attack, int mod)
{
	bool danger = bd.heroDanger;
	if (bd.heroModifiers[mod].danger == GenericCondition::TRUE && !danger)
	{
		return false;
	}
	if (bd.heroModifiers[mod].danger == GenericCondition::FALSE && danger)
	{
		return false;
	}
	if (bd.heroModifiers[mod].move == PlayerAttackMovement::any ||
		bd.heroModifiers[mod].move == bd.heroAttacks[attack].move)
	{
		if (bd.heroModifiers[mod].range == PlayerAttackRange::any ||
			bd.heroModifiers[mod].range == bd.heroAttacks[attack].range)
		{
			return true;
		}
	}

	return false;
}

void AdvanceAction(Entity &e)
{
	e.action.action++;
	e.action.actionProgress = 0;
	e.action.actionStart = true;
	if (e.action.startActions)
	{
		if (e.action.action >= e.action.actions.s_actions.size())
		{
			e.action.action = 0;
			e.action.startActions = false;

		}
	}
	else
	{
		if (e.action.playerColActions)
		{
			if (e.action.action >= e.action.actions.pc_actions.size())
			{
				e.action.action = 0;
				e.action.playerColActions = false;
			}
		}
		else
		{
			if (e.action.action >= e.action.actions.actions.size())
			{
				if (e.action.loopActions)
				{
					e.action.action = 0;
				}
				else
				{
					e.action.done = true;
				}

			}
		}

	}
}

Entity SummonEnemy(int hp, int type, int enemyType, float stageWidth, float stageHeight, int size,
	std::vector<Entity> &enemies,
	//std::vector<Entity *> &entities,
	Vector2D badPosition)
{
	Entity e;
	e.hp = hp;
	e.maxHP = hp;
	e.type = type;
	//if (hp <= 0) e.machine.state = States::estEnemyDeadA;

	e.position.Set(rand() % (int)stageWidth, rand() % (int)stageHeight);
	auto disSq = (e.position - badPosition).MagnitudeSqr();
	if (disSq < 80 * 80)
	{
		if (e.position.x < 150)
			e.position.x += 150;
		else
			e.position.x -= 150;

	}
	e.size.Set(size, size);

	return e;
	//entities.push_back(&enemies.back());
}

void ApplyPowerUp(PowerUp pu, powerup::BattleModifierMod mod)
{
	if (pu.tag != "")
	{
		bd.powerUpTagsAttained.push_back(pu.tag);
	}
	for (size_t i = 0; i < powerUpMaxMod; i++)
	{
		if (pu.mod[i].type != BattleModifierType::NONE)
		{
			bd.heroModifiers.push_back(pu.mod[i]);
			bd.heroModifiersMods.push_back(mod);
		}

		if (pu.mod[i].type == BattleModifierType::lifeIncrease)
		{
			bd.enemyFollowLimit += pu.mod[i].amount;
		}
		if (pu.mod[i].type == BattleModifierType::bulletTimeCDMultiplier)
		{
			bd.heroBulletTimeCD *= pu.mod[i].amount;
		}
		if (pu.mod[i].type == BattleModifierType::bulletTimeTimeMultiplier)
		{
			bd.heroBulletTimeMax *= pu.mod[i].amount;
		}
		if (pu.mod[i].type == BattleModifierType::combolength)
		{
			bd.maxCombo++;
		}
		if (pu.mod[i].type == BattleModifierType::attackUnlock)
		{
			for (size_t atk = 0; atk < bd.heroAttacks.size(); atk++)
			{
				if (heroModifierMatchAttack(atk, bd.heroModifiers.size() - 1))
				{

					bd.heroAttacks[atk].active = true;
				}
			}
		}
	}

}

int& BattleLoop_CheckWantGiveResource()
{
	return bd.wantToGiveResource;
}

void BattleLoop_ShowBattleTextOnHero(std::string txt)
{
	BattleText bt;
	bt.tr = TextRender(txt.c_str());
	bt.pos = bd.Hero().position;
	//bt.time = 0.3f;
	bd.battleTexts.push_back(bt);

	//throw gcnew System::NotImplementedException();
}

void BattleLoop_BasicInit()
{
	SDL_Log("basic init");
	if (enemyDefeatedAmount_pers.size() != 30)
		enemyDefeatedAmount_pers.resize(30);

	SetBackgroundColorSDL(color_backgroundB);


	{
		BattleData bData;
		bd = bData;
	}

	int hpBonus = 0;
	for (size_t i = 0; i < powerup::activeSpirits.size(); i++)
	{
		auto sp = powerup::joinedSpirits[powerup::activeSpirits[i]];
		hpBonus += sp.hpBase + sp.hpPerLevel * sp.level;
	}


	Entity hero;
	bd.ents[0].push_back(hero);
	bd.Hero().position.Set(90 + 10, 80);
	bd.Hero().size.Set(8, 8);
	bd.Hero().hp = 20 + hpBonus;
	bd.Hero().maxHP = bd.Hero().hp;
	bd.Hero().action.beatPassAction = false;
	bd.Hero().machine.state = estHero_Still;

	{

		ParticleSystem ps;//プレイやーの動きエフェクト
		ps.position = Vector2D(30, 30);
		ps.burst = 0;
		ps.size = Vector2D(3, 2);
		ps.shape.size = 4;
		ps.emissionRate = 20;
		ps.sizeOverLifeTime = Vector2D(1, 0);
		ps.life = Vector2D(2.1f, 2.1f);
		ps.initialColor = color_hero;
		bd.heroTrailAcc = bd.particleExecuter.StartSystem(ps);
	}
	efo_reset();
}



//#init
void BattleLoop_BattleInit()
{



	powerup::Init();

	powerup::RecalculatePermanentPowerUps();



	positionOffsetSDL.Set(0, 0);

	//default battle data if cannot read gamedata.txt
	EnemyGroupSpawn groupEnemySpawn[groupEnemySpawnSize] =
	{
		{-1,  5,  6, 0},
		{-1,  6,  2, 1},
		{ 3, mx,  5, 1},
		{ 7, mx,  7, 1},
		{ 5, 14, 15, 0},
		{ 2,  8,  1, 2},
		{ 5, 14,  5, 2},
		{ 7, mx, 10, 2},
		{ 5, 15,  1, 3},
		{ 7, 20,  2, 3},
		{10, mx,  3, 3},
		{12, mx,  6, 3}
	};

	//#hero #skill #heroskill
	{
		ActionHolder ah;
		{
			action::Action a(action::Type::heroSkillMode, 0);
			a.bonly = true;
			ah.actions.push_back(a);
		}
		{
			action::Action a;
			a.type = action::Type::TargetClosestEnemy;
			ah.actions.push_back(a);
		}
		{
			action::Action a(action::Type::playerInputEnable, 0);
			a.bonly = false;
			ah.actions.push_back(a);
		}
		{
			action::Action a(action::Type::imortal, 0);
			a.bonly = true;
			ah.actions.push_back(a);
		}
		{
			action::Action a(action::Type::LookAtTarget, 0);
			ah.actions.push_back(a);
		}
		{
			action::Action a(action::Type::Nothing, 5 / 30.0f);
			ah.actions.push_back(a);
		}
		/*{
			action::Action a(action::Type::Advance, 0.1f);
			a.rmove.speed = 600;
			ah.actions.push_back(a);
		}*/
		{
			action::Action a(action::Type::Advance, 0.6f);
			a.rmove.speed = -30;
			ah.actions.push_back(a);
		}
		{
			action::Action a(action::Type::Advance, 0.05f);
			a.rmove.speed = 300;
			ah.actions.push_back(a);
		}
		{
			action::Action a(action::Type::Advance, 0.01f);
			a.rmove.speed = 20;
			ah.actions.push_back(a);
		}
		{
			action::Action a(action::Type::pushbackTarget, 0.0f);
			a.intOnly = 30;
			ah.actions.push_back(a);
		}
		{
			action::Action a(action::Type::damageTarget, 0.0f);
			a.intOnly = 55;
			ah.actions.push_back(a);
		}
		{
			action::Action a(action::Type::pushbackTargetOnDirection, 0.0f);
			a.intOnly = 1000;
			ah.actions.push_back(a);
		}
		{
			action::Action a(action::Type::Advance, 0.2f);
			a.rmove.speed = 300;
			ah.actions.push_back(a);
		}
		{
			action::Action a(action::Type::playerInputEnable, 0);
			a.bonly = true;
			ah.actions.push_back(a);
		}
		{
			action::Action a(action::Type::Nothing, 0.6f);
			a.bonly = true;
			ah.actions.push_back(a);
		}
		{
			action::Action a(action::Type::heroSkillMode, 0);
			a.bonly = false;
			ah.actions.push_back(a);
		}
		{
			action::Action a(action::Type::imortal, 0);
			a.bonly = false;
			ah.actions.push_back(a);
		}
		bd.heroSkillDatabase.push_back(ah);
	}


	//default battle data if cannot read gamedata.txt
	{
		{
			using namespace action;
			std::vector<Action> actions;
			Action a(Type::RandomMove, 0.5f);
			a.time = 0.4f;
			a.rmove.speed = 80;
			actions.push_back(Action(Type::Nothing, 0.5f));
			actions.push_back(a);
			ActionHolder ah;
			ah.actions = actions;
			bd.entityActionDatabase[1].push_back(ah);
		}
		{
			using namespace action;
			std::vector<Action> actions;
			Action a(Type::FollowHero, 1.0f);
			//Action a(Type::RandomMove, 1.0f);
			a.time = 0.4f;
			a.rmove.speed = 30;
			//actions.push_back(Action(Type::Nothing, 0.3f));
			actions.push_back(a);
			ActionHolder ah;
			ah.actions = actions;
			bd.entityActionDatabase[1].push_back(ah);
		}
		{
			using namespace action;
			std::vector<Action> actions;
			actions.push_back(Action(Type::Nothing, 0.4f));
			{
				Action a(Type::ShootHero, 0.4f);
				a.shero.shotSpeed = 50.0f;
				actions.push_back(Action(Type::Nothing, 2.0f));
				actions.push_back(a);
			}
			{
				Action a(Type::RandomMove, 1.0f);
				a.time = 0.4f;
				a.rmove.speed = 80;
				actions.push_back(Action(Type::Nothing, 0.3f));
				actions.push_back(a);
			}
			ActionHolder ah;
			ah.actions = actions;
			bd.entityActionDatabase[1].push_back(ah);
		}
		{
			using namespace action;
			std::vector<Action> actions;
			//actions.push_back(Action(Type::Nothing, 0.3f));
			{
				Action a(Type::ShootHero, 1.2f);
				a.shero.shotSpeed = 80.0f;
				actions.push_back(a);
			}
			//actions.push_back(Action(Type::Nothing, 1.5f));
			ActionHolder ah;
			ah.actions = actions;
			bd.entityActionDatabase[1].push_back(ah);
		}
	}
	//basic player attack data
	{
		{
			PlayerAttackData attackData;
			attackData.initRange = 30;
			attackData.attack = baseDamage * 3;
			attackData.rangeIncreaseRatio = 0;
			attackData.move = PlayerAttackMovement::still;
			attackData.range = PlayerAttackRange::close;
			bd.heroAttacks.push_back(attackData);
		}
		{
			PlayerAttackData attackData;
			attackData.initRange = 45;
			attackData.rangeIncreaseRatio = 0.0f;
			attackData.attack = baseDamage * 2;
			attackData.move = PlayerAttackMovement::still;
			attackData.range = PlayerAttackRange::medium;
			attackData.active = true;
			bd.heroAttacks.push_back(attackData);

		}
		{
			PlayerAttackData attackData;
			attackData.initRange = 60;
			attackData.rangeIncreaseRatio = 0.0f;
			attackData.attack = baseDamage;
			attackData.move = PlayerAttackMovement::still;
			attackData.range = PlayerAttackRange::far;
			bd.heroAttacks.push_back(attackData);
		}
		{
			PlayerAttackData attackData;
			attackData.initRange = 30;
			attackData.attack = baseDamage * 3;
			attackData.rangeIncreaseRatio = 0;
			attackData.move = PlayerAttackMovement::moving;
			attackData.range = PlayerAttackRange::close;
			bd.heroAttacks.push_back(attackData);
		}
		{
			PlayerAttackData attackData;
			attackData.initRange = 45;
			attackData.rangeIncreaseRatio = 0.0f;
			attackData.attack = baseDamage * 2;
			attackData.move = PlayerAttackMovement::moving;
			attackData.range = PlayerAttackRange::medium;
			attackData.active = true;
			bd.heroAttacks.push_back(attackData);
		}
		{
			PlayerAttackData attackData;
			attackData.initRange = 60;
			attackData.rangeIncreaseRatio = 0.0f;
			attackData.attack = baseDamage;
			attackData.move = PlayerAttackMovement::moving;
			attackData.range = PlayerAttackRange::far;
			attackData.active = false;
			bd.heroAttacks.push_back(attackData);
		}
	}



	for (size_t i = 0; i < groupEnemySpawnSize; i++)
	{
		bd.groupEnemySpawn.push_back(groupEnemySpawn[i]);
	}

	//#data #gamedata #load
	std::string str = FileUtils::ReadFromFile("assets/gamedata.txt");
	bd = ReadBattleData(bd, str);

	{ //remove this TODO
		PathBattleWave pbw;
		pbw.start = 2;
		pbw.length = 2;
		pbw.level = 2;
		pbw.startPathId = -1;
		pbw.end = 5;
		//bd.paths.push_back(pbw);

	}


	for (size_t i = 0; i < ENTTYPEAMOUNT; i++)
	{
		bd.renders[i].resize(bd.entitySummonDataSource[i].size());
	}

	//setup artifact graphics
	{

		{
			int artifactId = ResolveAlias("artifact_weaponA");
			ImageRegionDraw ir;
			ir.ir.imageIndex = 1;
			ir.ir.origin = { 32,16, 16, 16 };
			ir.ir = AtlasManager::GetImage("powerup10001");

			ir.scaleX = 1;
			ir.scaleY = 1;
			bd.renders[3][artifactId].regions.push_back(ir);
			//bd.renders[3][artifactId].regions.push_back();
		}
		{
			int artifactId = ResolveAlias("artifact_weaponB");
			ImageRegionDraw ir;
			ir.ir.imageIndex = 1;
			ir.ir.origin = { 0,0, 16, 16 };
			ir.ir = AtlasManager::GetImage("squarepowerup0000");

			ir.scaleX = 1;
			ir.scaleY = 1;
			bd.renders[3][artifactId].regions.push_back(ir);
		}
		{
			int artifactId = ResolveAlias("artifact_weaponC");
			ImageRegionDraw ir;
			ir.ir.imageIndex = 1;
			ir.ir.origin = { 48,0, 16, 16 };
			ir.ir = AtlasManager::GetImage("powerup50000");

			ir.scaleX = 1;
			ir.scaleY = 1;
			bd.renders[3][artifactId].regions.push_back(ir);
			//bd.renders[3][artifactId].text = "Test";
		}
	}

	//hero attack effects
	{
		float duration = 0.03f * 12;
		for (size_t j = 0; j < 3; j++)
		{

			int frames = 9;
			if (j == 0) frames = 13;
			std::string s = "danced_";
			if (j == 1) s = "danceb_";
			if (j == 2) s = "dancec_";

			SpriteSheetEffect sse;
			for (size_t i = 0; i < frames; i++)
			{
				ImageRegionDraw ir;

				auto s2 = s + std::to_string(i);
				ir.ir = AtlasManager::GetImage(s2.c_str());

				sse.regions.push_back(ir);
			}
			sse.inactiveEnd = false;
			//sse.timePerImage
			//F * p = T
			sse.timePerImage = duration / frames;
			bd.ssEffect.push_back(sse);
		}
	}

	//various sprite sheet effects
	{
		const int efn = 6;
		int frames[efn] = { 5, 20,5,7, 7, 6 };
		int framesPerSecond[efn] = { 30,30,30,30, 40, 30 };
		std::string starts[efn] = { "damage2a_","musicnote_", "damage1_", "skilltrigger_", "attacktell_", "slashb_" };
		for (size_t i = 0; i < efn; i++)
		{
			int frame = frames[i];
			auto s = starts[i];

			SpriteSheetEffect sse;
			for (size_t i = 0; i < frame; i++)
			{
				ImageRegionDraw ir;

				auto  s2 = s + std::to_string(i);
				ir.ir = AtlasManager::GetImage(s2.c_str());

				sse.regions.push_back(ir);
			}
			sse.timePerImage = 1.0f / framesPerSecond[i];
			bd.ssEffectDB.push_back(sse);
		}
	}
	{
		SpriteSheetEffect sse;
		for (size_t i = 0; i < 2; i++)
		{
			ImageRegionDraw ir;
			ir.ir = AtlasManager::GetImage("musicnote2");
			ir.scaleX = 1;
			ir.scaleY = 1;
			sse.regions.push_back(ir);
		}
		sse.inactiveEnd = false;
		sse.loop = true;
		bd.ssEffectDB.push_back(sse);
	}


	//weapon artifact summoner
	{
		EntitySummonData esd;
		esd.entityType = 3;
		esd.entityId = 0;
		esd.hp = 1;
		esd.size = 0;
		bd.entitySummons.push_back(esd);

	}

	//resource artifact summoner
	{
		EntitySummonData esd;
		esd.entityType = 3;
		esd.entityId = ResolveAlias("artifact_resource_spawn");
		esd.hp = 1;
		esd.size = 0;
		bd.entitySummons.push_back(esd);

	}




	ParticleSystem enemyBlast1, enemyBlast2;
	{
		enemyBlast1.burst = 1;
		enemyBlast1.life = Vector2D(0.06f, 0.06f);
		enemyBlast1.shape.size = 1;
		enemyBlast1.size = Vector2D(50, 50);
		enemyBlast1.initialColor = color_enemydeath;

		enemyBlast2.burst = 10;
		enemyBlast2.shape.size = enemyBlast1.size.x / 2;
		enemyBlast2.size = Vector2D(4, 2);
		enemyBlast2.sizeOverLifeTime = Vector2D(1, 0);
		enemyBlast2.life = Vector2D(2.0f, 1.0f);
		enemyBlast2.radialSpeed = Vector2D(3, 8);
		enemyBlast2.initialColor = color_enemyDead;
	}


	ParticleSystem enemySpawn1, enemySpawn2;
	{

		enemySpawn1.burst = 1;
		enemySpawn1.life = Vector2D(0.1f, 0.1f);
		enemySpawn1.shape.size = 4;
		enemySpawn1.size = Vector2D(50, 50);
		enemySpawn1.initialColor = color_enemy;


		enemySpawn2.burst = 20;
		enemySpawn2.shape.size = enemyBlast1.size.x / 2;
		enemySpawn2.size = Vector2D(3, 1);
		enemySpawn2.sizeOverLifeTime = Vector2D(1, 0);
		enemySpawn2.life = Vector2D(2.3f, 2.0f);
		enemySpawn2.radialSpeed = Vector2D(4, 20);
		enemySpawn2.initialColor = color_enemy;
	}
	bd.enemyBlast1 = enemyBlast1;
	bd.enemyBlast2 = enemyBlast2;
	bd.enemySpawn1 = enemySpawn1;
	bd.enemySpawn2 = enemySpawn2;

	//remove TODO remove this
	if (powerup::permanentPowerUps.size() == 0 && false)
	{
		powerup::permanentPowerUps.push_back(powerup::powerUpDatabase[0]);
		auto bmm = powerup::BattleModifierMod();
		bmm.addProbability = 0.1f;
		powerup::permanentPowerUpMods.push_back(bmm);
	}


	for (size_t i = 0; i < powerup::permanentPowerUps.size(); i++)
	{
		ApplyPowerUp(powerup::permanentPowerUps[i], powerup::permanentPowerUpMods[i]);
	}

	////remove this TODO
	//powerup::SpiritJoin(0);
	//powerup::SpiritJoin(1);
	//powerup::SpiritJoin(5);

}

Vector2D BattleLoop_ConstraintPosition(int minX, int minY, int maxX, int maxY, Entity* enemy)
{
	Vector2D reverse = { 1,1 };
	if (enemy->position.x < enemy->size.x / 2 + minX)
	{
		enemy->position.x = minX + enemy->size.x / 2;
		reverse.x = -1;
	}
	if (enemy->position.x > maxX - enemy->size.x / 2) {
		reverse.x = -1;
		enemy->position.x = maxX - enemy->size.x / 2;
	}

	if (enemy->position.y > maxY - enemy->size.y / 2) {
		enemy->position.y = maxY - enemy->size.y / 2;
		reverse.y = -1;
	}
	if (enemy->position.y < minY + enemy->size.y / 2)
	{
		reverse.y = -1;
		enemy->position.y = minY + enemy->size.y / 2;
	}
	return reverse;
}

Vector2D ResolvePosition(action::PositionData posA, Entity &e)
{
	Vector2D pos;
	Vector2D offsetv = posA.offset;
	offsetv.rotate(posA.rotateOffset);
	//position solver code
	for (size_t i = 0; i < 2; i++)
	{
		float p = posA.source.x;
		auto type = posA.posTypeX;
		if (i == 1)
		{
			p = posA.source.y;
			type = posA.posTypeY;
		}
		switch (type)
		{
		case action::PositionType::Self:
			if (i == 0)
			{
				pos.x = e.position.x + p;
			}
			else
			{
				pos.y = e.position.y + p;
			}
			break;
		case action::PositionType::Random:
		{
			if (i == 0)
			{
				pos.x = rand() % (int)bd.stageWidth;
			}
			else
			{
				pos.y = rand() % (int)bd.stageHeight;
			}

		}
		break;
		case action::PositionType::Screen:
			if (i == 0)
			{
				pos.x = p * bd.stageWidth;
			}
			else
			{
				pos.y = p * bd.stageHeight;
				//pos.y = e.position.y + p;
			}
			break;
		case action::PositionType::Hero:
			if (i == 0)
			{
				pos.x = bd.Hero().position.x + p;
			}
			else {
				pos.y = bd.Hero().position.y + p;
			}
			break;
		default:
			break;
		}
	}
	pos = pos + offsetv;
	return pos;

}

void BattleLoop_RoomMode()
{
	bd.stageHeight = 9999;
	bd.stageWidth = 9999;
}

Vector2D cameraOffset;

BattleResult BattleLoop_Loop()
{

	if (bd.heroCamera)
		//if(true)
	{
		Vector2D desiredoffset;

		//desiredoffset.Set((int)widthSDL / 2, (int)(heightSDL / 2));
		desiredoffset.Set((int)widthSDL / 2, (int)(300 / 2));
		desiredoffset += bd.Hero().position*-1;
		desiredoffset.x = (int)desiredoffset.x;
		desiredoffset.y = (int)desiredoffset.y;
		/*if ((positionOffsetSDL - desiredoffset).MagnitudeSqr() >1)
		{*/
		int x = desiredoffset.x;
		int y = desiredoffset.y;
		/*y = (y / 2) * 2;
		x = (x / 2) * 2;*/

		auto diff = cameraOffset - desiredoffset;

		int maxDis = 50;
		int speed = -100;
		auto ms = diff.MagnitudeSqr();
		if (diff.MagnitudeSqr() > maxDis*maxDis)
		{

			cameraOffset = desiredoffset + diff.normalize() * (maxDis);
			//SDL_Log("%d",(int)diff.MagnitudeSqr());
			//speed *= 4;
		}
		else
		{
			//SDL_Log("%d", (int)diff.MagnitudeSqr());
			speed = speed;
		}
		cameraOffset += diff.normalize()*speed * deltaTimeSDL;
		if (ms < 3 && ms >0)
		{
			cameraOffset.x = x;
			cameraOffset.y = y;
		}
		positionOffsetSDL.x = (int)cameraOffset.x;
		positionOffsetSDL.y = (int)cameraOffset.y;


		/*positionOffsetSDL.x = x;
		positionOffsetSDL.y = y;*/

		//}
		//if()

	}
	int heroSkillTrigger = -1;
	bd.wantToSave = false;

	//remove this
	if (IsKeyDownFrame(SDLK_u))
	{
		heroSkillTrigger = 0;
	}

	if (bd.Hero().hp <= 0 && bd.state != BattleState::heroDeath)
	{
		ChangeState(BattleState::heroDeath);
		StopMusicSDL();

	}
	if (bd.timeOnState > 3.0f && bd.state == BattleState::heroDeath)
	{
		return BattleResult::gameover;
	}
	float timeDeathStop = 18.0f / 30.0f;
	if (bd.state == BattleState::heroDeath && bd.timeOnState > timeDeathStop && bd.Hero().active)
	{
		ScreentintSDL(color_enemy, 0.2f);
		ParticleSystem heroDeathBlast;
		heroDeathBlast.burst = 60;
		heroDeathBlast.shape.size = 4;
		heroDeathBlast.size = Vector2D(0.4, 3);
		heroDeathBlast.sizeOverLifeTime = Vector2D(1, 0);
		heroDeathBlast.life = Vector2D(2.0f, 1.0f);
		heroDeathBlast.radialSpeed = Vector2D(10, 240);
		heroDeathBlast.initialColor = color_hero;
		heroDeathBlast.position = bd.Hero().position;
		bd.particleExecuter.StartSystem(heroDeathBlast);
		bd.Hero().active = false;
	}

	float speedAdvanceMultiplier = 1.0f;
	if (bd.pathId >= 0) {
		if (bd.paths[bd.pathId].level > 0)
		{
			// deltaTimeSDL *= 1.0f;
			speedAdvanceMultiplier = 1.3f;
		}
	}


	bd.beatPeriodC = bd.beatPeriodC - deltaTimeSDL;
	bd.beatPeriodC2 = bd.beatPeriodC2 - deltaTimeSDL;
	bool beatHappen = false;
	bool beat2Happen = false;
	if (bd.beatPeriodC2 < 0)
	{
		beat2Happen = true;
		bd.beatPeriodC2 += bd.beatPeriod;
		//PlaySoundSDL(1);
	}
	if (bd.beatPeriodC < 0)
	{
		beatHappen = true;
		bd.beatCount++;
		bd.beatDurationC = bd.beatDuration;
		bd.beatPeriodC += bd.beatPeriod;
		// PlaySoundSDL(0);
		if (RandomRange(0, 1) > 0.92f)
			efo_swap();
	}
	if (bd.beatDurationC >= 0)
	{
		bd.beatDurationC -= deltaTimeSDL;
	}
	if (renderScreenTransition)
	{
		UpdateScreenTransition(deltaTimeSDL);
		if (renderScreenTransition)
			RenderScreenTransition();
	}

	if (Profiler::enabled)
	{
		//if (IsKeyDownFrame(SDLK_m))//キーボードを読む
		//{
		//	bd.allEnemyPoison = 10000;
		//}
		//if (IsKeyDownFrame(SDLK_n))//キーボードを読む
		//{
		//	bd.Hero().hp = 0;
		//}
	}

	Profiler::StartTime("SDL");
	int sdlLoop = Loop_SDL(); //SDL backend code
	Profiler::EndTime("SDL");
	auto storyUnlockNew = StoryUnlockedAndRecalculateStoryGoal();


	if (storyUnlockNew && bd.storyUnlockLastFrame == false)
	{
		bd.wantStoryUnlockThisBattle = true;
		bd.wantToSave = true;
	}

	bd.storyUnlockLastFrame = storyUnlockNew;

	if (Profiler::debugCommandInput)
	{
		if (Profiler::debugCommand == "storygo")
		{
			bd.wantStoryUnlockThisBattle = true;
		}
		if (Profiler::debugCommand == "joinme")
		{
			bd.joinProbBase = 0.5f;
		}
	}

	deltaTimeSDLOriginal = deltaTimeSDL;
	deltaTimeSDL *= bd.heroBulletTime;
	deltaTimeSDL *= bd.deltaTimeMultiplier;
	float deltaTimeSDL_BulletTime = deltaTimeSDL;

	if (bd.heroBulletTimeCDcounter >= 0)
		bd.heroBulletTimeCDcounter -= deltaTimeSDLOriginal;
	if (bd.heroBulletTimeMaxCounter > 0)
	{
		bd.heroBulletTimeMaxCounter -= deltaTimeSDLOriginal;
		if (bd.heroBulletTimeMaxCounter <= 0)
		{
			bd.heroBulletTimeCDcounter = bd.heroBulletTimeCD;
			bd.heroBulletTime = 1.0f;
		}
	}


	if (bd.state != BattleState::normal
		&& bd.state != BattleState::storyChoice
		&& bd.state != BattleState::levelUpChoice
		&& bd.state != BattleState::pathChoice
		&& bd.state != BattleState::levelUpMessage
		&& bd.state != BattleState::xpCollection
		&& bd.state != BattleState::peace
		&& bd.state != BattleState::afterBattle
		&& bd.state != BattleState::waveui
		)
	{
		if (bd.state == BattleState::heroDeath && bd.timeOnState > timeDeathStop) {

		}
		else
		{
			deltaTimeSDL = 0;
		}

	}

	float frameRate = 1.0f / deltaTimeSDL;

	if (frameRate < 70) //フレームレートを図る
	{
		//SDL_Log("FRAME DROP");
		//SDL_Log("%f", frameRate);
	}
	Profiler::StartTime("P_update");
	bd.particleExecuter.Update(deltaTimeSDL_BulletTime);
	Profiler::EndTime("P_update");
	if (sdlLoop < 0) //end game code　ゲームの終わり条件
	{
		return BattleResult::closeGame; //end game
	}
	bd.battleTime += deltaTimeSDL;
	bd.timeToSummonEnemy -= deltaTimeSDL;

	if (bd.timeToSummonEnemy < 0 && bd.state == BattleState::normal)
	{
		ChangeState(BattleState::afterBattle);
		bd.clearedWave = bd.wave;
		bd.wantToSave = true;
	}

	if (bd.timeToSummonEnemy < 0 && bd.xpEffects.size() > 0 && bd.state == BattleState::afterBattle)
	{
		ChangeState(BattleState::xpCollection);
	}

	if (bd.wantStoryUnlockThisBattle && bd.state == BattleState::afterBattle)
	{

		return BattleResult::storyWantToUnlock;
	}

	if (bd.state == BattleState::afterBattle && bd.enemyIdToJoin >= 0)
	{
		ChangeState(BattleState::spiritacquisition);

	}

	std::vector<int> pathIdsSelec;
	if (bd.state == BattleState::pathChoice || bd.state == BattleState::afterBattle)
	{

		for (size_t i = 0; i < bd.paths.size(); i++)
		{
			//if (bd.pathId == -1) 
			if (bd.paths[i].startPathId == bd.pathId)
			{
				if (bd.paths[i].start == bd.wave)
				{
					bool refused = false;
					for (size_t j = 0; j < bd.refusedPaths.size(); j++)
					{
						if (bd.refusedPaths[j] == i)
						{
							refused = true;
							break;
						}
					}
					if (!refused)
						pathIdsSelec.push_back(i);
				}
			}

		}

	}
	if (bd.state == BattleState::afterBattle)
	{
		if (bd.pathId >= 0)
		{
			if (bd.paths[bd.pathId].length == bd.wave) //end wave	
			{
				bd.wave = bd.paths[bd.pathId].end;
				bd.pathId = -1;

			}
		}


		if (pathIdsSelec.size() > 0)
		{
			BattleChoices::Reset();
			BattleChoices::choices.push_back(BattleChoices::ChoicesE::easyPath);
			BattleChoices::choices.push_back(BattleChoices::ChoicesE::hardPath);
			BattleChoices::FeedChoices();
			BattleChoices::OverwriteDescription("The usual battles", 0);
			BattleChoices::OverwriteDescription("Harder enemies await", 1);
			ChangeState(BattleState::pathChoice);

		}
	}


	if (bd.state == BattleState::afterBattle)
	{
		ChangeState(BattleState::waveui);
	}

	if (bd.state == BattleState::advancewave)//code for checking battle state
	{
		ChangeState(BattleState::normal);
		int nextWave = bd.wave + 1;

		for (size_t i = 0; i < bd.fixedWaves.size(); i++)
		{
			if (bd.fixedWaves[i].wave == nextWave && bd.fixedWaves[i].pathId == bd.pathId)
			{
				if (bd.fixedWaves[i].title.size() > 0) {
					//wave has title
					bd.waveForTitle = i;
					ChangeState(BattleState::bossName);
				}
			}
		}
	}

	xpReq = 5 + bd.heroLevel * 20;

	bool waveTitle = false;
	bool beatingWaveTitle = false;
	bool canSummon = true;
	bool xpCollect = false;
	if (bd.state == BattleState::xpCollection)
	{
		canSummon = false;
		xpCollect = true;
	}
	if (bd.state == BattleState::levelUpMessage)
	{
		xpCollect = true;
		canSummon = false;
		SDL_Color color = color_damagingA;
		if (fmod(bd.timeOnState, bd.beatPeriod) < bd.beatDuration && true)
		{
			color = color_dance;
		}
		DrawTextSDL(TextRender("LEVEL UP", bd.stageWidth / 2, bd.stageHeight*0.3f, color, 2));
		if (bd.timeOnState > 2.0f)
		{
			ChangeState(BattleState::levelUpChoice);
			bd.hideHeroTime = 0.6f;

			BattleChoices::Reset();
			BattleChoices::choices.push_back(BattleChoices::ChoicesE::powerUp1);
			BattleChoices::choices.push_back(BattleChoices::ChoicesE::powerUp2);
			BattleChoices::FeedChoices();
			BattleChoices::OverwriteText(powerup::powerUpDatabase[bd.powerUpForChoice.at(0)].name, 0);
			BattleChoices::OverwriteText(powerup::powerUpDatabase[bd.powerUpForChoice.at(1)].name, 1);
			BattleChoices::OverwriteDescription(powerup::powerUpDatabase[bd.powerUpForChoice.at(0)].description, 0);
			BattleChoices::OverwriteDescription(powerup::powerUpDatabase[bd.powerUpForChoice.at(1)].description, 1);
		}
	}
	if (xpCollect)
	{
		for (size_t i = 0; i < bd.xpEffects.size(); i++)
		{
			auto xpef = bd.xpEffects[i];
			auto ef = &bd.ssEffect[xpef];
			auto dis = bd.Hero().position - ef->pos;
			ef->pos = ef->pos + (dis).normalize() * 250 * deltaTimeSDL;
			if (dis.MagnitudeSqr() < 10)
			{
				bd.ssEffect[xpef].active = false;
				bd.xpEffects.erase(bd.xpEffects.begin() + i);
				i--;
				bd.xpAbsorbed += 1;
				bd.xpField -= 1;
			}
		}

		auto sum = [](int x, int y) { return x + y; };
		auto VerifyIfHasSimilarModifier = [](BattleModifier bm) {
			for (size_t i = 0; i < bd.heroModifiers.size(); i++)
			{
				if (bd.heroModifiers[i].type == bm.type)
				{
					if (bd.heroModifiers[i].range == bm.range)
					{
						if (bd.heroModifiers[i].move == bm.move)
						{
							return true;
						}
					}
				}
			}
			return false;
		};
		if (bd.xpAbsorbed > xpReq)
		{
			bd.xpAbsorbed -= xpReq;
			bd.heroLevel++;
			bd.enemyFollowLimit++;

			std::vector<int> powerUpPool;
			for (size_t powerupId = 0; powerupId < powerup::powerUpDatabase.size(); powerupId++)
			{
				PowerUp pu = powerup::powerUpDatabase[powerupId];

				//do not get any power ups that can be gotten in spirits
				//for now...
				if (pu.spiritRarity >= 0) continue;

				for (size_t i = 0; i < pu.forbiddenTags.size(); i++)
				{
					for (size_t atag = 0; atag < bd.powerUpTagsAttained.size(); atag++)
					{
						if (pu.forbiddenTags[i] == bd.powerUpTagsAttained[atag])
						{
							goto nogood;
						}

					}
				}
				for (size_t i = 0; i < pu.necessaryTags.size(); i++)
				{
					bool tagFound = false;
					for (size_t atag = 0; atag < bd.powerUpTagsAttained.size(); atag++)
					{
						if (pu.necessaryTags[i] == bd.powerUpTagsAttained[atag])
						{
							tagFound = true;
							break;
						}
					}
					if (!tagFound)
					{
						goto nogood;
					}
				}
				powerUpPool.push_back(powerupId);
			nogood:;
			}

			bd.powerUpForChoice.clear();
			for (size_t i = 0; i < 2; i++)
			{
				int index = RandomRangeInt(0, powerUpPool.size());
				bd.powerUpForChoice.push_back(powerUpPool[index]);
				powerUpPool.erase(powerUpPool.begin() + index);
			}

			ChangeState(BattleState::levelUpMessage);
		}
		else
		{
			if (bd.xpEffects.size() == 0 && bd.state == BattleState::xpCollection)
			{
				ChangeState(BattleState::afterBattle);
			}
		}

	}

	if (bd.state == BattleState::peace)
	{
		canSummon = false;
	}

	if (bd.state == BattleState::bossName)
	{
		waveTitle = true;
		beatingWaveTitle = true;
		canSummon = false;

		if (bd.timeOnState > 2.0f)
		{
			ChangeState(BattleState::bossSummon);
		}
	}

	if (bd.state == BattleState::spiritacquisition)
	{
		canSummon = false;
		{
			TextRender tr("I wish to join you!", widthSDL / 2, 120);
			DrawTextSDL(tr, 4);
			ImageRegionDraw ird = bd.renders[1][bd.enemyIdToJoin].regions[0];
			ird.y = 160;
			ird.x = tr.x;
			AddRegionDraw(ird, 4);
			if (bd.timeOnState > 2)
			{
				ChangeState(BattleState::afterBattle);
				if (bd.enemyIdToJoin >= 0)
				{
					powerup::SpiritJoin(bd.enemyIdToJoin);
					bd.wantToSave = true;
					bd.enemyIdToJoin = -1;
				}
			}

		}
	}

	if (bd.state == BattleState::waveui)
	{
		canSummon = false;
		int x = 4;
		int widthT = 0;
		int previousX;

		int sizeL = 2;
		int sizeH = 5;

		for (size_t i = 0; i <= bd.maxWave; i++)
		{
			int w = sizeL;
			for (size_t fx = 0; fx < bd.fixedWaves.size(); fx++)
			{
				if (i == bd.fixedWaves[fx].wave)
				{
					w = 5;
					break;
				}
			}
			widthT += w + 2;

		}



		for (size_t path = 0; path < 2; path++)
		{
			if (bd.pathId < 0 && path == 1) break;
			int maxWave = bd.maxWave;
			int pathIdForDraw = -1;
			int currentWave = bd.wave;
			x = 4;
			if (path == 0 && bd.pathId >= 0)
			{
				currentWave = bd.paths[bd.pathId].start;
			}
			if (path == 1)
			{
				maxWave = bd.paths[bd.pathId].length;
				pathIdForDraw = bd.pathId;
			}
			for (size_t i = 0; i <= maxWave; i++)
			{
				int w = sizeL;
				int h = sizeL;
				int y = 30;
				for (size_t fx = 0; fx < bd.fixedWaves.size(); fx++)
				{
					if (i == bd.fixedWaves[fx].wave && bd.fixedWaves[fx].pathId == pathIdForDraw)
					{
						w = 5;
						h = 5;
						break;
					}
				}
				int offset = (widthSDL - widthT) / 2;
				int yoff = path * 40;
				SDL_Rect r = { x - w / 2 + offset, y - h / 2 + yoff, w, h };
				AddRectangleS(r, color_enemy, 8);
				bool drawHero = false;
				int bonusHeight = 0;
				int bonusWidth = 0;
				if (i == (currentWave) && false)
				{
					drawHero = true;
				}
				if (i == (currentWave + 1))
				{
					drawHero = true;
					bonusHeight = 5;
					bonusWidth = 0;
				}

				if (drawHero)
				{

					r.h = 4 + bonusHeight;
					r.w = 2 + bonusWidth;
					r.x = x - r.w / 2 + offset;
					r.y = y + h / 2 + r.h / 2 + 3 - r.h / 2 + yoff;
					AddRectangleS(r, color_hero, 8);
					r.y = y - h / 2 - r.h / 2 - 3 - r.h / 2 + yoff;
					AddRectangleS(r, color_hero, 8);
				}
				x += w + 2;
				previousX = x;


			}

		}



		if (bd.timeOnState > 1.9f)
		{
			ChangeState(BattleState::advancewave);
		}
	}

	if (bd.state == BattleState::storyChoice)
	{
		canSummon = false;
	}
	if (bd.state == BattleState::levelUpChoice)
	{
		canSummon = false;
	}
	if (bd.state == BattleState::pathChoice)
	{
		canSummon = false;
	}
	if (bd.state == BattleState::bossSummon)
	{
		waveTitle = true;
		canSummon = true;
		if (bd.timeOnState > 0.8f)
		{
			ChangeState(BattleState::normal);
		}
	}

	if (waveTitle)
	{
		auto wave = bd.fixedWaves[bd.waveForTitle];
		auto title = wave.title;
		SDL_Color color = color_damagingA;
		if (fmod(bd.timeOnState, bd.beatPeriod) < bd.beatDuration && beatingWaveTitle)
		{
			color = color_dance;
		}
		DrawTextSDL(TextRender(title.c_str(), bd.stageWidth / 2, bd.stageHeight*0.3f, color, 1));
		DrawTextSDL(TextRender(wave.subtitle.c_str(), bd.stageWidth / 2, bd.stageHeight*0.3f + 25, color_projharmless, 1));
	}

	bool tutorialOn = enemyDefeatedAmount_pers[0] == 0;


	//enemy summoning code 
	if (bd.timeToSummonEnemy < 0 && canSummon && bd.groupEnemySpawn.size() > 0)//敵の召喚コード
	{
		bd.wantToSave = true;
		bd.wave++;
		if (bd.wave > bd.maxWave)
		{
			return BattleResult::clear;
		}
		if (bd.fightingFixedWave == true)  //was fighting fixed wave befor
		{
			bd.score *= 2;
		}
		bd.fightingFixedWave = false;

		if (tutorialOn)
		{
			EntitySummonData esd;
			esd.entityId = 0;
			esd.entityType = 1;
			esd.hp = 30;
			esd.size = 16;
			esd.position.Set(bd.stageWidth / 2, enemyTutorialY);
			esd.actionEnabled = false;
			bd.entitySummons.push_back(esd);

			bd.timeToSummonEnemy = 400;
		}
		if (tutorialOn == false)
		{
			for (size_t i = 0; i < bd.fixedWaves.size(); i++)
			{
				if (bd.fixedWaves[i].wave == bd.wave && bd.fixedWaves[i].pathId == bd.pathId)
				{
					bd.fightingFixedWave = true;
					bd.timeToSummonEnemy = 120;
					auto ea = bd.fixedWaves[i].enemyAmounts;
					auto ei = bd.fixedWaves[i].enemyIds;
					for (size_t i = 0; i < ea.size(); i++)
					{
						for (size_t j = 0; j < ea[i]; j++)
						{
							if (bd.entitySummonDataSource[1].size() <= ei[i])
							{
								SDL_Log("MEMORY PROBLEM");
							}
							auto esd = bd.entitySummonDataSource[1][ei[i]];
							bd.entitySummons.push_back(esd);
						}

					}
				}
			}

			if (bd.fightingFixedWave == false)
			{
				std::vector<int> possibleWaves;
				for (size_t i = 0; i < bd.groupEnemySpawn.size(); i++)
				{
					auto gr = bd.groupEnemySpawn[i];
					int pathLevel = -1;
					if (bd.pathId >= 0)
					{
						pathLevel = bd.paths[bd.pathId].level;
					}
					if (gr.minWave < bd.wave && gr.maxWave > bd.wave && gr.pathLevel == pathLevel)
					{
						possibleWaves.push_back(i);
					}
				}
				if (possibleWaves.size() != 0)
				{
					int numberOfGroups = 1;
					int maxGroups = 1;
					if (bd.wave > 1) maxGroups = 2;
					if (bd.wave > 5) maxGroups = 3;
					if (bd.wave > 10) maxGroups = 4;
					numberOfGroups += bd.wave % maxGroups;
					int hpSummoned = 0;
					for (size_t i = 0; i < numberOfGroups; i++)
					{
						auto waveSummon = bd.groupEnemySpawn[possibleWaves[RandomRangeInt(0, possibleWaves.size())]];
						for (size_t j = 0; j < waveSummon.nEnemies; j++)
						{
							if (bd.entitySummonDataSource[1].size() <= waveSummon.enemyType)
							{
								SDL_Log("MEMORY PROBLEM");
							}
							auto esd = bd.entitySummonDataSource[1][waveSummon.enemyType];
							hpSummoned += esd.hp;
							bd.entitySummons.push_back(esd);
						}

					}

					bd.timeToSummonEnemy = hpSummoned * 2;
				}

			}
		}
	}

	bool summonEnabled = true;
	if (bd.state == BattleState::peace)
	{
		summonEnabled = false;
	}
	if (summonEnabled)
	{
		//#summon #spawn
		for (size_t i = 0; i < bd.entitySummons.size(); i++)
		{
			auto esd = bd.entitySummons[i];
			int insertionPlace = -1;
			Entity e;
			if (esd.entityType == 1)  //enemy
			{
				//SDL_Log("%d", bd.ents[1].size());
				e = SummonEnemy(esd.hp, 1, esd.entityId, bd.stageWidth, bd.stageHeight, esd.size, bd.ents[1], bd.Hero().position);
				e.machine.state = estEnemyAlive;
				//SDL_Log("%d", bd.ents[1].size());
				bd.enemySpawn1.position = e.position;
				bd.enemySpawn2.position = e.position;
				bd.particleExecuter.StartSystem(bd.enemySpawn1);
				bd.particleExecuter.StartSystem(bd.enemySpawn2);
			}
			else
			{
				//SDL_Log("Not enemy");
			}

			for (size_t i = 0; i < bd.ents[esd.entityType].size(); i++)
			{
				if (bd.ents[esd.entityType][i].active == false)
				{
					insertionPlace = i;
				}
			}
			if (insertionPlace == -1)
			{
				bd.ents[esd.entityType].push_back(e);
				insertionPlace = bd.ents[esd.entityType].size() - 1;
			}
			e.type = esd.entityType;
			e.id = esd.entityId;
			e.hp = esd.hp;
			e.tag = esd.tag;
			e.destroyOnDamaging = esd.destroyOnDamageable;
			e.action.acting = esd.actionEnabled;
			if (esd.summoner.id >= 0)
			{
				e.side = bd.ents[esd.summoner.type][esd.summoner.id].side;
				/*if (e.side > 0)
				{
					SDL_Log("Summoning wrong side!");
				}*/
			}
			if (e.type == 1)
			{
				e.side = -1;
			}
			if (e.type == 3)
			{
				e.side = 1;
			}
			e.size = Vector2D(esd.size, esd.size);
			e.action.actions = bd.entityActionDatabase[esd.entityType][esd.entityId];
			e.speedMultiplier = esd.speedMultiplier;
			if (esd.spawnEntity.spawnAsChild)
			{
				e.parent = esd.summoner;
			}
			e.enemyDamageable = esd.spawnEntity.enemyDamageable;
			e.direction = { 1,0 };
			if (esd.spawnEntity.angle != 0)
			{
				e.direction.rotate(esd.spawnEntity.angle);
				e.rotationOffset = esd.spawnEntity.angle;
			}



			if (esd.position.x != -99999)
			{
				//auto lastE = bd.ents[esd.entityType].back();
				e.position = esd.position;
			}


			bd.ents[esd.entityType][insertionPlace] = e;


			//bd.enemies.back().action.actions = bd.entityActionDatabase[1][bd.enemies.back().enemyType];

		}
		bd.entitySummons.clear();
	}


	//bd.hero = bd.entities.Get(0);
	//SDL_Log("Delta: %lf",deltaTimeSDL);
	int speed = 120;
	if (movementMode == 0) dir.Set(0, 0);
	bool moved = false;

	if (bd.playerInputOn && bd.Hero().hp >= 0)
	{
		if (IsKeyPressed(SDLK_DOWN) || IsKeyPressed(SDLK_s))//キーボードを読む
		{
			moved = true;
			dir.y = 1;
			if (movementMode == 1)dir.x = 0;
		}
		if (IsKeyPressed(SDLK_UP) || IsKeyPressed(SDLK_w))
		{
			moved = true;
			dir.y = -1;
			if (movementMode == 1)dir.x = 0;
		}
		if (IsKeyPressed(SDLK_LEFT) || IsKeyPressed(SDLK_a))
		{
			moved = true;
			dir.x = -1;
			if (movementMode == 1)dir.y = 0;
		}
		if (IsKeyPressed(SDLK_RIGHT) || IsKeyPressed(SDLK_d))
		{
			moved = true;
			dir.x = 1;
			if (movementMode == 1)dir.y = 0;
		}
		if (cursorPressedSDL)
		{

			auto mouseDis = Vector2D(cursorXSDL, cursorYSDL) - Vector2D(cursorStartXSDL, cursorStartYSDL);
			float dis = 50;
			float minDis = 10;
			float minSpeed = 10;
			float minSpeedRatio = 10 / speed;
			auto mg = mouseDis.length();
			mg = mg - minDis;
			if (mg > (dis - minDis))
			{
				mg = dis;
			}
			if (mg > 0)
			{
				moved = true;
				dir = mouseDis;
				float scale = (mg / (dis - minDis)*(1 - minSpeedRatio)) + minSpeedRatio;
				dir = dir.normalize() * scale;
			}

			SDL_Rect r;

			r.w = 6;
			r.h = 6;
			r.x = cursorStartXSDL - r.w / 2;
			r.y = cursorStartYSDL - r.h / 2;

			AddRectangleS(r, color_hero, 6);

			//dir = dir.normalize();
		}

	}

	if (bd.hideHeroTime <= 0)
	{
		if (bd.forbiddenLeftDown.x == 1 && dir.x < 0) {
			dir.x = 0;
		}
		if (bd.forbiddenLeftDown.y == 1 && dir.y > 0) {
			dir.y = 0;
		}
		if (bd.forbiddenUpRight.y == 1 && dir.y < 0) {
			dir.y = 0;
		}
		if (bd.forbiddenUpRight.x == 1 && dir.x > 0) {
			dir.x = 0;
		}
		bd.Hero().position.x = bd.Hero().position.x + dir.x* speed * deltaTimeSDL;
		bd.Hero().position.y = bd.Hero().position.y + dir.y* speed * deltaTimeSDL;
		if (bd.Hero().position.x < 0) bd.Hero().position.x = 0;
		if (bd.Hero().position.x > bd.stageWidth) bd.Hero().position.x = bd.stageWidth;
		if (bd.Hero().position.y > bd.stageHeight) bd.Hero().position.y = bd.stageHeight;
		if (bd.Hero().position.y < 0) bd.Hero().position.y = 0;
	}

	//bd.Hero().position.Set(bd.Hero().position.x, bd.Hero().position.y);
	//bd.entities.Update(0, bd.hero);

	//プレイヤーのステートマシン
	if (moved) { //change state to moving
		if (bd.Hero().machine.state == estHero_Still)
		{
			bd.Hero().machine.ChangeState(estHero_Movement);
			if (bd.heroBulletTime == 1 && movementMode == 0) {
				bd.Hero().combo = 0;
			}

		}

	}
	else
	{
		if (bd.Hero().machine.state == estHero_Movement)
		{
			bd.Hero().machine.ChangeState(estHero_Still);

			//bd.Hero().timeToAttack = 0.1f;
		}
	}

	//hero attack code #hero #attack #heroattack
	//bd.Hero().timeToAttack -= deltaTimeSDL;
	auto heroAttacks = bd.heroAttacks;
	std::vector<float> disToEnemy(bd.ents[1].size());

	bool heroBeatAttack = bd.beatCount % bd.heroBeatAttack == 0;
	if (beatHappen
		&& heroBeatAttack
		&& bd.Hero().damageStagInvincible <= 0 && bd.Hero().hp > 0 && !bd.heroSpecialAttackOn)
	{
		bool stillAttack = bd.Hero().machine.state == estHero_Still;
		if (bd.heroBulletTime != 1)
		{
			stillAttack = true;
		}
		if (movementMode == 1)
			stillAttack = true;

		float baseRangeMultiplier = 1.0f;
		if (bd.Hero().combo == 1) baseRangeMultiplier = 1.5f;
		if (bd.Hero().combo == 2) baseRangeMultiplier = 2.0f;

		std::vector<int> heroAttackRange(heroAttacks.size());

		//calculate range
		for (size_t j = 0; j < heroAttacks.size(); j++)
		{
			int trueRange = heroAttacks[j].initRange + heroAttacks[j].rangeIncreaseRatio*bd.oldProgress;
			float rangeMultiplier = baseRangeMultiplier;
			for (size_t bm = 0; bm < bd.heroModifiers.size(); bm++)
			{
				if (bd.heroModifiers[bm].type == BattleModifierType::rangeMultiplier
					&& heroModifierMatchAttack(j, bm))
				{
					rangeMultiplier *= powerup::CalculateAmount(bm, 1);


				}
			}
			trueRange *= rangeMultiplier;
			heroAttackRange.at(j) = trueRange;
		}

		//bd.Hero().timeToAttack = 0.26f;
		bool hit = false;
		//deal damage
		for (size_t i = 0; i < bd.ents[1].size(); i++)
		{
			if (bd.ents[1][i].hp > 0 && bd.ents[1][i].enemyDamageable)
			{
				Vector2D dis = bd.Hero().position - bd.ents[1][i].position;
				float disF = dis.length() - bd.ents[1][i].size.x / 2;
				int attackHit = -1;
				for (size_t j = 0; j < heroAttacks.size(); j++)
				{
					if (!heroAttacks[j].active) continue;
					int trueRange = heroAttackRange[j];
					if (trueRange > disF && (heroAttacks[j].move == PlayerAttackMovement::moving) == !stillAttack)
					{
						attackHit = j;
						break;
					}
				}
				if (attackHit >= 0)
				{
					if (RandomRange(0, 1) < 0.3f && bd.ents[1][i].maxHP > 10)
					{
						auto source = bd.entitySummonDataSource[3][ResolveAlias("artifact_skillA")];

						auto offset = (bd.ents[1][i].position - bd.Hero().position);
						if (RandomRange(0, 1) < 0.5f)
							offset.rotate(90);
						else
							offset.rotate(-90);
						source.position = offset.normalize()*(30 + bd.ents[1][i].size.x) + bd.ents[1][i].position;

						source.spawnEntity.spawnAsChild = true;
						source.summoner = EntityRef(1, i);
						bd.entitySummons.push_back(source);
					}

					int strengthPush = 75 * 5;

					if (bd.ents[1][i].pushbackStr*bd.ents[1][i].pushbackStr < strengthPush*strengthPush)
					{
						bd.ents[1][i].pushback = dis.normalize() * -1;
						bd.ents[1][i].pushbackStr = strengthPush;
						bd.ents[1][i].pushbackPriority = 1;
					}

					hit = true;
					EntityDamage ed;
					int damageAdd = 0;
					for (size_t hm = 0; hm < bd.heroModifiers.size(); hm++)
					{

						if (bd.heroModifiers[hm].type == BattleModifierType::damageAdd)
						{
							if (heroModifierMatchAttack(attackHit, hm)) {

								damageAdd += powerup::CalculateAmount(hm, 0);
							}

						}

					}
					int damage = (heroAttacks[attackHit].attack + damageAdd) * (bd.Hero().combo*1.0f + 1);
					for (size_t hm = 0; hm < bd.heroModifiers.size(); hm++)
					{

						if (bd.heroModifiers[hm].type == BattleModifierType::damageMultiplier)
						{
							if (heroModifierMatchAttack(attackHit, hm)) {

								damage *= powerup::CalculateAmount(hm, 1);

							}
						}

					}
					ed.damage = (int)(damage);
					ed.entity = EntityRef(1, i);
					bd.damages.push_back(ed);

				}
			}
		}



		bool renderAttack = false;
		if (!bd.attackOnlyOnDistance)
		{
			renderAttack = true;
		}
		if (hit)
		{
			renderAttack = true;
		}
		else
		{
			//PlaySoundSDL(playerSoundMiss, 0);
		}

		//attack render
		if (renderAttack)
		{
			if (stillAttack)
			{
				if (bd.Hero().combo < bd.maxCombo)
				{
					bd.Hero().combo++;
				}
				else
				{
					bd.Hero().combo = 0;
				}
			}
			//render attack graphic
			for (size_t j = 0; j < heroAttacks.size(); j++)
			{
				if (!heroAttacks[j].active) continue;
				if ((heroAttacks[j].move == PlayerAttackMovement::moving) == !stillAttack)
				{

					int effect = 2;

					{
						int initRange = heroAttacks[j].initRange;
						if (initRange <= 50)
						{
							effect = 1;
						}
						if (initRange <= 30)
						{
							effect = 0;
						}
						float rangeMultiplier = (float)heroAttackRange[j] / 30;
						bd.ssEffect[effect].pos = bd.Hero().position;
						bd.ssEffect[effect].scale = { rangeMultiplier, rangeMultiplier };
						bd.ssEffect[effect].progress = 0;
						bd.ssEffect[effect].counter = 0;
					}


					int range = heroAttackRange[j] - 10;

					//bd.gameEffects.push_back(Effect(efAttack, range , &bd.hero, 0.1f));
					ParticleSystem ps;
					ps.shape.size = range;
					ps.life = Vector2D(0.1f, 0.2f)* bd.heroBeatAttack;
					ps.burst = 60;
					int attackForParticleCombo = heroAttacks[j].attack;
					attackForParticleCombo = (int)(attackForParticleCombo*(1 + bd.Hero().combo*2.0f));
					//ps.size = Vector2D((attackForParticleCombo) * 3 + 2, (attackForParticleCombo) * 2 + 1);
					ps.size = Vector2D(3, 1);
					ps.initialColor = color_enemydeath;
					ps.initialColor.b -= (3 - heroAttacks[j].attack) * 40;
					ps.initialColor.g += (3 - heroAttacks[j].attack) * 80;
					ps.sizeOverLifeTime = Vector2D(1, 0.0f);
					ps.radialSpeed = Vector2D(1.0f, 2.0f);
					ps.position = bd.Hero().position;
					//bd.particleExecuter.StartSystem(ps);

				}
			}

		}
	}

	for (size_t i = 0; i < bd.damages.size(); i++)
	{
		if (bd.damages[i].entity.id < 0) {
			continue; //this should never happen
		}
		int takeDamageUnit = bd.damages[i].entity.id;
		int parentId = bd.ents[1][bd.damages[i].entity.id].parent.id;
		while (parentId >= 0)
		{
			takeDamageUnit = parentId;
			parentId = bd.ents[1][parentId].parent.id;
		}
		bd.ents[1][takeDamageUnit].hp = bd.ents[1][takeDamageUnit].hp - bd.damages[i].damage;
		bd.ents[1][takeDamageUnit].cooldownValues[0] = 0.12f;
		BattleText bt;
		bd.lastEnemyHitId = takeDamageUnit;
		bt.pos = bd.ents[1][takeDamageUnit].position + Vector2D(RandomRangeInt(-10, 10), RandomRangeInt(-10, 10));
		bt.tr = TextRender(std::to_string(bd.damages[i].damage).c_str(), 0, 0);

		bd.battleTexts.push_back(bt);
		//bd.gameEffects.push_back(Effect(efDamageEnemy, bd.enemies[i].size.x + 4, &bd.enemies[i], 0.1f));
		//int j = attackHit;
		//bd.gameEffects.push_back(Effect(efAttack, range , &bd.hero, 0.1f));
		ParticleSystem ps;
		ps.shape.size = 0.1f;
		int size = bd.ents[1][bd.damages[i].entity.id].size.x + 8 * bd.damages[i].damage;
		ps.life = Vector2D(0.1f, 0.12f);
		ps.burst = 1;
		ps.size = Vector2D(size, size);
		ps.initialColor = color_enemydeath;
		/*ps.initialColor.b -= (3 - heroAttacks[attackHit].attack) * 40;
		ps.initialColor.g += (3 - heroAttacks[attackHit].attack) * 80;*/
		ps.sizeOverLifeTime = Vector2D(0.8f, 1.0f);
		ps.position = bd.ents[1][bd.damages[i].entity.id].position;
		//bd.particleExecuter.StartSystem(ps);

		auto scale = bd.damages[i].damage / (7.0f*5.0f);
		int effectId[] = { 0 };
		if (scale > 1)
		{
			scale /= 3;
			if (scale > 0.5f)
			{
				scale = 0.5f;
			}
			effectId[0] = 2;
		}


		for (size_t effect = 0; effect < 1; effect++)
		{
			auto sse = bd.ssEffectDB[effectId[effect]];
			sse.pos = bd.ents[1][bd.damages[i].entity.id].position;
			if (effect == 0)
			{
				sse.scale = { scale, scale };
				if (RandomRange(0, 1.0f) > 0.5f)
				{
					sse.scale.x = -sse.scale.x;
				}
				if (RandomRange(0, 1.0f) > 0.5f)
				{
					sse.scale.y = -sse.scale.y;
				}
			}

			if (true)
			{
				bool added = false;
				for (size_t i = 0; i < bd.ssEffect.size(); i++)
				{
					if (bd.ssEffect[i].inactiveEnd)
					{
						if (bd.ssEffect[i].progress >= bd.ssEffect[i].regions.size())
						{
							bd.ssEffect[i] = sse;
							added = true;
							break;
						}
					}
				}
				if (added == false)
				{
					bd.ssEffect.push_back(sse);
				}
			}


		}

	}
	if (bd.damages.size() > 0)
	{
		PlaySound(SoundEnum::enemydamage);
	}
	bd.damages.clear();
	Profiler::StartTime("proj loop");
	//projectile code
	//敵のたまの管理コード #proj
	for (size_t i = 0; i < bd.ents[2].size(); i++)
	{
		Entity* p = &bd.ents[2][i];
		if (p->active == false) continue;
		Vector2D disR = bd.ents[2][i].velocity * deltaTimeSDL;
		p->position = p->position + disR;

		/*if (p.damaging && (p.position - bd.Hero().position).MagnitudeSqr() < p.size.x*p.size.x)
		{
			bd.Hero().hp = bd.Hero().hp - 1;
		}*/
		//bd.ents[2][i] = p;
	}
	Profiler::EndTime("proj loop");


	//SDL_Log("%d", bd.entities.size());
	//enemy loop code
	//敵の管理コード
	efo_update(deltaTimeSDL, bd.Hero());
	int numberOfEnemiesAlive = 0;
	int enemySound = -1;
	int enemySoundChannel = 0;
	Profiler::StartTime("enemy loop");
	for (size_t i = 0; i < bd.ents[1].size(); i++)
	{

		Entity* enemy = &bd.ents[1][i]; //optimize this
		if (enemy->active == false) continue;
		if (enemy->machine.state == States::estEnemyAlive)
		{
			numberOfEnemiesAlive++;

			enemy->hp -= (int)(bd.allEnemyPoison * deltaTimeSDL);

			Vector2D reverse(1, 1);
			int minY = 0;
			int minX = 0;
			int maxX = bd.stageWidth;
			int maxY = bd.stageHeight;

			reverse = BattleLoop_ConstraintPosition(minX, minY, maxX, maxY, enemy);
			enemy->pushback.x *= reverse.x;
			enemy->pushback.y *= reverse.y;
		}
		if (enemy->machine.state == estEnemyDeadA && enemy->machine.timeInState >= 1)
		{
			enemy->machine.ChangeState(estEnemyDeadB);
		}
		if (enemy->machine.state == estEnemyDeadB)
		{
			if (enemy->size.x > 10)
			{
				float sizeRed = deltaTimeSDL * 20;
				enemy->size.x -= sizeRed;
				enemy->size.y -= sizeRed;
			}
		}
		if (enemy->machine.state == estEnemyDeadB && enemy->machine.timeInState >= 2)
		{
			if (false && efo_followPositions.size() < bd.enemyFollowLimit)
			{

				efo_addfollower();
				enemy->machine.ChangeState(estEnemyDeadFollowing);
			}
			else
			{
				enemy->machine.ChangeState(estEnemyDeadLeaving);
				enemy->direction.Set(1, 0);
				enemy->direction.rotate(RandomRangeInt(0, 360));

			}
		}
		if (enemy->machine.state == estEnemyDeadLeaving)
		{
			enemy->position += enemy->direction * 90 * deltaTimeSDL;
			if (enemy->machine.timeInState >= 3)
			{
				enemy->active = false;
			}
		}

		//dead enemy follow bd.hero
			//倒された敵はプレイヤーをフォローする仕組み

		if (enemy->hp <= 0
			&& enemy->machine.state == estEnemyAlive
			&& enemy->active == true
			&& (enemy->pushbackStr < 10 && enemy->pushbackStr > -10)
			)
		{
			enemyDefeatedAmount_pers[enemy->id]++;
			//e.active = false;

			enemy->damaging = false;
			bd.oldProgress += 1;
			bd.score += enemy->size.x*enemy->size.x;
			int xp = 0;
			if (enemy->maxHP > 0)
				xp = 1;

			if (enemy->maxHP > 10 * enemyHPMultiply)
			{
				xp += 1;
			}
			if (enemy->maxHP > 50 * enemyHPMultiply)
			{
				xp += 10;
			}
			if (enemy->maxHP > 200 * enemyHPMultiply)
			{
				xp += 15;
			}
			bd.xpField += xp;
			bd.totalXP += xp;

			if (powerup::CanASpiritJoin()) {
				float joinProb = bd.joinProbBase;
				if (bd.wave > 10 && bd.enemiesJoinedAmount == 0 && powerup::desperateForSpiritsJoin())
					joinProb *= 10;

				if (powerup::CanASpiritJoin()) {

					if (RandomRange(0, 1) < joinProb)
					{
						bd.enemyIdToJoin = enemy->id;
						bd.enemiesJoinedAmount++;
					}
				}
			}

			for (size_t musicNote = 0; musicNote < xp; musicNote++)
			{
				int effect = 1;
				auto sse = bd.ssEffectDB[(int)BattleEffects::DatabaseEffects::musicNoteXP];
				sse.inactiveEnd = false;
				int size = enemy->size.x;
				size -= 5;

				sse.pos = enemy->position;
				if (size > 0)
					sse.pos += Vector2D(RandomRange(-size, size), RandomRange(-size, size));
				int effectId = BattleEffects::AddEffect(sse);
				bd.xpEffects.push_back(effectId);

			}

			//if (bd.enemies[i].) bd.xp += 2;
			bd.enemyBlast1.position = enemy->position;
			bd.enemyBlast2.position = enemy->position;
			//bd.particleExecuter.StartSystem(bd.enemyBlast1);
			bd.particleExecuter.StartSystem(bd.enemyBlast2);
			enemy->machine.ChangeState(estEnemyDeadA);
			//e.state.ChangeState();

			for (size_t musicNote = 0; musicNote < 3; musicNote++)
			{
				int effect = 1;
				auto sse = bd.ssEffectDB[effect];
				int size = enemy->size.x;
				size -= 5;

				sse.pos = enemy->position;
				if (size > 0)
					sse.pos += Vector2D(RandomRange(-size, size), RandomRange(-size, size));
				sse.delay = (musicNote)*0.1f;
				bool added = false;
				for (size_t i = 0; i < bd.ssEffect.size(); i++)
				{
					if (bd.ssEffect[i].inactiveEnd)
					{
						if (bd.ssEffect[i].progress >= bd.ssEffect[i].regions.size())
						{
							bd.ssEffect[i] = sse;
							added = true;
							break;
						}
					}
				}
				if (added == false)
				{
					bd.ssEffect.push_back(sse);
				}
			}

			{
				int effect = 0;
				auto sse = bd.ssEffectDB[effect];
				int size = enemy->size.x;


				sse.pos = enemy->position;
				auto sc = size / 18.0f;
				if (sc > 1) sc = 1;
				sse.scale = { sc, sc };
				sse.timePerImage = 0.016f * 3;
				//sse.delay = (musicNote)*0.1f;
				bool  added = false;
				for (size_t i = 0; i < bd.ssEffect.size(); i++)
				{
					if (bd.ssEffect[i].inactiveEnd)
					{
						if (bd.ssEffect[i].progress >= bd.ssEffect[i].regions.size())
						{
							bd.ssEffect[i] = sse;
							added = true;
							break;
						}
					}
				}
				if (added == false)
				{
					bd.ssEffect.push_back(sse);
				}
			}


			PlaySound(SoundEnum::enemydead);
		}
		if (enemy->hp <= 0 && enemy->type == 1 && enemy->active && enemy->machine.state == estEnemyDeadFollowing)
		{

			efo_follow(*enemy, bd.Hero(), deltaTimeSDL);

		}
	}
	Profiler::EndTime("enemy loop");
	/*if (enemySound >= 0)
		PlaySoundSDL(enemySound, enemySoundChannel);*/
	if (numberOfEnemiesAlive == 0 && bd.timeToSummonEnemy > 1)
	{
		bd.timeToSummonEnemy = 1;
	}
	bd.particleExecuter.SetSystemPosition(bd.heroTrailAcc, bd.Hero().position);
	if (bd.Hero().hp <= 0)
	{
		bd.particleExecuter.SetEmission(bd.heroTrailAcc, 0);
	}

	heroMinDisToDamage = 400;
	//entity general logic execution code #logic #collision #action
	Profiler::StartTime("entity loop A");
	for (size_t k = 0; k < ENTTYPEAMOUNT; k++)
	{
		//auto bd.ents[k] = bd.ents[k];
		for (int i = 0; i < bd.ents[k].size(); i++)
		{
			//auto e = bd.ents[k][i];
			if (bd.ents[k][i].pushbackStr != 0)
			{
				int immune = bd.ents[k][i].pushbackImmunityPriority;
				if (bd.ents[k][i].damaging == true && immune == 0)
				{
					immune = 1;
				}
				if (
					//bd.ents[k][i].damaging == false 
					true
					&& bd.ents[k][i].parent.id < 0
					&& immune < bd.ents[k][i].pushbackPriority)
					bd.ents[k][i].position += bd.ents[k][i].pushback * bd.ents[k][i].pushbackStr * deltaTimeSDL;
				bool positive = bd.ents[k][i].pushbackStr > 0;
				float attriction = 3000;
				if (positive)
					bd.ents[k][i].pushbackStr -= deltaTimeSDL * attriction;
				else if (bd.ents[k][i].pushbackStr != 0)
					bd.ents[k][i].pushbackStr += deltaTimeSDL * attriction;
				bool positiveAfter = bd.ents[k][i].pushbackStr > 0;
				if (positive != positiveAfter)
				{
					bd.ents[k][i].pushbackStr = 0;
					bd.ents[k][i].pushback.Set(0, 0);
				}
			}

			if (bd.ents[k][i].damageStagInvincible > 0)
			{
				bd.ents[k][i].damageStagInvincible -= deltaTimeSDL;

			}

			if (bd.ents[k][i].parent.id >= 0)
			{
				if (bd.ents[bd.ents[k][i].parent.type][bd.ents[k][i].parent.id].hp <= 0)
				{
					bd.ents[k][i].hp = -1;
					if (k == 3)  //artifact disappear #artifact
					{
						bd.ents[k][i].active = false;
					}
					//bd.ents[k][i] = e;
					continue;
				}
			}



			bd.ents[k][i].machine.timeInState += deltaTimeSDL;

			//#collision
			bool checkCollision = false;
			bool checkDistance = false;
			float leeway = 10;
			bool e2core = true;
			bool damaging = bd.ents[k][i].damaging && bd.ents[k][i].active && bd.ents[k][i].side < 0;
			bool playerActionCollision = bd.ents[k][i].active&& bd.ents[k][i].action.actions.pc_actions.size() > 0 && bd.ents[k][i].action.playerColActions == false;
			if (damaging)
			{
				checkCollision = true;
				checkDistance = true;
			}
			if (playerActionCollision)
			{
				checkCollision = true;
				leeway = 0;
				e2core = false;
			}
			if (checkCollision)
			{
				Entity *e1 = &bd.ents[k][i];
				Entity *e2 = &bd.Hero();



				Vector2D leew = Vector2D(leeway, leeway);
				auto nsize1 = e1->size - leew;
				float minsize = 2.0f;
				if (nsize1.x < minsize)nsize1.x = minsize;
				if (nsize1.y < minsize)nsize1.y = minsize;

				bool colliding = true;
				Vector2D min1 = e1->position - nsize1 * 0.5f;
				Vector2D max1 = e1->position + nsize1 * 0.5f;
				Vector2D min2 = e2->position - e2->size*0.5f;
				Vector2D max2 = e2->position + e2->size*0.5f;

				if (e1->position == e1->positionAfterCollisionCheck)
				{
				}
				else  //if object moved between frames
				{
					//if they moved, you should calculate the square that emcompasses both squares
					//so you get the smallest origin possible and then get the full size (doubled)
					Vector2D positionN = e1->position;
					if (positionN.x < e1->positionAfterCollisionCheck.x)
						positionN.x = e1->positionAfterCollisionCheck.x;
					if (positionN.y < e1->positionAfterCollisionCheck.y)
						positionN.y = e1->positionAfterCollisionCheck.y;
					min1 = positionN - nsize1 * 0.5f;
					max1 = positionN + nsize1 * 0.5f;
				}

				if (e2core)
				{
					min2 = e2->position;
					max2 = e2->position;
				}

				if (checkDistance)
				{
					//the dis portion of the code is to calculate the minimal distance to danger the hero has
					float dis1 = min1.x - max2.x;
					float dis2 = -max1.x + min2.x;
					float dis3 = min1.y - max2.y;
					float dis4 = -max1.y + min2.y;
					if (dis1 < 0)dis1 *= -1; if (dis2 < 0)dis2 *= -1; if (dis3 < 0)dis3 *= -1; if (dis4 < 0)dis4 *= -1;
					float disx = dis1;
					float disy = dis3;
					if (disx > dis2)
					{
						disx = dis2;
					}
					if (disy > dis4)
					{
						disy = dis4;
					}
					float disT = disx + disy;
					if (heroMinDisToDamage > disT)
					{
						heroMinDisToDamage = disT;
					}
				}

				if (min1.x > max2.x || max1.x < min2.x)
				{
					colliding = false;
				}
				else
				{
					if (min1.y > max2.y || max1.y < min2.y)
					{
						colliding = false;
					}
					else
					{
						colliding = true;
					}

				}

				if (colliding)
				{
					if (damaging)
					{
						if (bd.Hero().damageStagInvincible <= 0 && bd.Hero().imortal == false)
						{
							bd.Hero().hp -= 5;
							bd.Hero().damageStagInvincible = 0.5f;
							ScreentintSDL({ 180, 64, 64 }, 0.1f);
							if (false)
							{
								if (efo_followPositions.size() >= 3)
								{
									efo_reduce(3);
									bd.Hero().damageStagInvincible = 0.5f;
									ScreentintSDL({ 180, 64, 64 }, 0.1f);
								}
								else
								{
									bd.Hero().hp -= 20;
								}
							}

						}
					}
					if (playerActionCollision && !bd.heroSpecialAttackOn)
					{
						bd.ents[k][i].action.playerColActions = true;
						bd.ents[k][i].action.action = 0;
						bd.ents[k][i].action.actionProgress = 0;
						bd.ents[k][i].action.actionStart = true;
					}


				}
			}

			bool damagingE = bd.ents[k][i].damaging && bd.ents[k][i].active && bd.ents[k][i].side > 0;
			bool checkCollisionE = false;
			if (damagingE)
				checkCollisionE = true;

			if (checkCollisionE)
			{
				for (size_t j = 0; j < bd.ents[1].size(); j++)
				{
					if (std::find(bd.ents[k][i].damagedEntityIds.begin(), bd.ents[k][i].damagedEntityIds.end(), j) != bd.ents[k][i].damagedEntityIds.end())
					{
						continue;
					}
					if (bd.ents[1][j].hp <= 0 || !bd.ents[1][j].active) continue;
					Entity *e1 = &bd.ents[k][i];
					Entity *e2 = &bd.ents[1][j];
					bool e2core = false;
					float leeway = 0;

					Vector2D leew = Vector2D(leeway, leeway);
					auto nsize1 = e1->size - leew;
					float minsize = 1.0f;
					if (nsize1.x < minsize)nsize1.x = minsize;
					if (nsize1.y < minsize)nsize1.y = minsize;

					bool colliding = true;
					Vector2D min1 = e1->position - nsize1 * 0.5f;
					Vector2D max1 = e1->position + nsize1 * 0.5f;
					Vector2D min2 = e2->position - e2->size*0.5f;
					Vector2D max2 = e2->position + e2->size*0.5f;

					if (e2core)
					{
						min2 = e2->position;
						max2 = e2->position;
					}
					if (min1.x > max2.x || max1.x < min2.x)
					{
						colliding = false;
					}
					else
					{
						if (min1.y > max2.y || max1.y < min2.y)
						{
							colliding = false;
						}
						else
						{
							colliding = true;
						}
					}
					if (colliding)
					{

						bd.ents[k][i].damagedEntityIds.push_back(j);
						EntityDamage ed;
						ed.damage = bd.ents[k][i].damagingAmount;
						ed.entity.id = j;
						ed.entity.type = 1;
						bd.damages.push_back(ed);
						//bd.ents[1][j].hp -= e.damagingAmount;
						if (bd.ents[k][i].destroyOnDamaging)
						{
							bd.ents[k][i].active = false;
							bd.ents[k][i].hp = -1;
							//bd.ents[k][i] = e;
						}
					}
				}
			}

			bd.ents[k][i].positionAfterCollisionCheck = bd.ents[k][i].position;

			//#action
			if (!bd.ents[k][i].action.acting)
				continue;
			if (bd.ents[k][i].active == false || bd.ents[k][i].hp <= 0) {
				//bd.ents[k][i] = e;
				continue;

			}
			if (bd.ents[k][i].action.done)
			{
				continue;
			}
			if (k == 1 && bd.heroSpecialAttackOn) { //enemy do not act during hero attack
				continue;
			}
			if (k == 2 && bd.heroSpecialAttackOn) { //proj
				if (bd.ents[k][i].side < 0)  //enemy proj do not act during hero attack
				{
					continue;
				}

			}
			auto actions = &bd.ents[k][i].action.actions;
			auto s_actions = &bd.ents[k][i].action.actions.s_actions;
			if (s_actions->size() == 0) bd.ents[k][i].action.startActions = false;
			if (bd.ents[k][i].action.actions.pc_actions.size() == 0) bd.ents[k][i].action.playerColActions = false;
			if (actions->actions.size() > 0)
			{
				int act = 0;
				action::Action action;
				while (true)
				{
					act = bd.ents[k][i].action.action;

					if (bd.ents[k][i].action.startActions)
					{
						action = bd.ents[k][i].action.actions.s_actions[act];
					}
					else
					{
						if (bd.ents[k][i].action.playerColActions) {
							action = bd.ents[k][i].action.actions.pc_actions.at(act);
						}
						else
							action = actions->actions[act];
					}
					auto checked = CheckBattleRequirement(action.reqs, bd.ents[k][i]);
					if (checked)
					{
						break;
					}
					else
					{
						AdvanceAction(bd.ents[k][i]);
					}
				}






				float prog = bd.ents[k][i].action.actionProgress;
				if (bd.ents[k][i].action.actionStart) //action start code
				{
					bd.ents[k][i].action.actionStart = false;
					//Code that modifies the action!!!
					if (action.timesUsable >= 0)
					{
						if (action.timesUsable == 0)
						{
							action.reqs.clear();
							action.type = action::Type::Nothing;
							action.time = 0;
						}
						action.timesUsable--;

					}

					//finish up and commit action modification
					if (bd.ents[k][i].action.startActions)
					{
						bd.ents[k][i].action.actions.s_actions[act] = action;
					}
					else
					{

						if (bd.ents[k][i].action.playerColActions)
						{
							bd.ents[k][i].action.actions.pc_actions[act] = action;
						}
						else
						{
							bd.ents[k][i].action.actions.actions[act] = action;
						}

					}

					//action start code switch #start
					switch (action.type)
					{
					case action::Type::dialog:
					{
						Dialog::ShowOrAdvanceDialog(action.intOnly, bd.ents[k][i].position);
					}
					break;
					case action::Type::attackTell:
					{
						if (action.intOnly == 1)
						{
							ColoredRect r;
							r.rect = { 0, 0,100, 100 };
							r.color = color_enemy;
							ColoredRect r2;
							r2.rect = { 0,0,0,0 };
							r2.color = color_enemy;
							CenterRectSDL(r.rect, bd.ents[k][i].position);
							CenterRectSDL(r2.rect, bd.ents[k][i].position);
							ColoredRects::InterpolateRect(r, r2, 0.25f);



						}
						if (action.intOnly == 0)
						{
							auto sse = bd.ssEffectDB[(int)BattleEffects::DatabaseEffects::AttackTell];
							sse.pos = bd.ents[k][i].position;// +Vector2D(0, -bd.ents[k][i].size.y);
							sse.layer = 6;
							sse.scale = sse.scale * 1;
							PlaySound(SoundEnum::enemytell);
							bd.ssEffect.push_back(sse);
						}

					}
					break;
					case action::Type::TargetClosestEnemy:
					{
						auto posSelf = bd.ents[k][i].position;
						int bestDis = 999999;
						int enemy = -1;
						for (size_t en = 0; en < bd.ents[1].size(); en++)
						{
							if (bd.ents[1][en].hp <= 0) continue;
							auto mg = (bd.ents[1][en].position - posSelf).MagnitudeSqr();
							if (mg < bestDis)
							{
								bestDis = mg;
								enemy = en;
							}
						}
						if (enemy >= 0)
						{
							bd.ents[k][i].target = EntityRef(1, enemy);
						}
					}
					break;
					case action::Type::imortal:
					{
						bd.ents[k][i].imortal = action.bonly;
					}
					break;
					case action::Type::playerInputEnable:
					{
						bd.playerInputOn = action.bonly;
					}
					break;
					case action::Type::heroSkillMode:
					{
						bd.heroSpecialAttackOn = action.bonly;
						if (bd.heroSpecialAttackOn)
						{
							bd.heroBulletTimeCDcounter = 0;
							bd.heroBulletTimeMaxCounter = 0;
							bd.heroBulletTime = 1;
						}
					}
					break;
					case action::Type::Disappear:
					{
						bd.ents[k][i].active = false;
					}
					break;
					case action::Type::skip:
					{
						bd.ents[k][i].action.action += action.intOnly;
					}
					break;
					case action::Type::eraseTag:
					{
						int tag = action.intArray[0];
						int type = action.intArray[1];
						for (size_t i = 0; i < bd.ents[type].size(); i++)
						{
							if (bd.ents[type][i].tag == tag)
							{
								bd.ents[type][i].active = false;
							}
						}
						//SDL_Log("sss");
					}
					break;
					case action::Type::TeleportHero:
					{
						if (action.boolD == false)
							ScreentintSDL({ 0,0,0,255 }, 0.12f);
						auto pos = ResolvePosition(action.positionOnly, bd.ents[k][i]);
						bd.Hero().position = pos;
						keyPressesSDL.clear();
					}
					break;
					case action::Type::teleport:
					{
						auto pos = ResolvePosition(action.positionOnly, bd.ents[k][i]);
						bd.ents[k][i].position = pos;
					}
					break;
					case action::Type::SpawnEntity:
					{
						auto se = action.spawnEntity;
						Vector2D pos;

						auto posA = se.pos;
						pos = ResolvePosition(se.pos, bd.ents[k][i]);
						if (se.entityType == 2 && k == 1)
						{
							PlaySound(SoundEnum::enemyshoot);
						}
						if (se.entityType >= 0)
						{
							EntitySummonData esd = bd.entitySummonDataSource[se.entityType][se.entityId];
							esd.spawnEntity = se;
							esd.position = pos;
							esd.summoner.type = k;
							esd.summoner.id = i;
							esd.speedMultiplier *= se.speedMul;
							bd.entitySummons.push_back(esd);
						}
						if (se.entityType == -1) //effect summoning
						{
							auto ef = bd.ssEffectDB[se.entityId];
							ef.pos = pos;
							bd.ssEffect.push_back(ef);
						}


					}
					break;
					case action::Type::DamageOnTouch:
					{
						bd.ents[k][i].damaging = action.bonly;
					}
					break;
					case action::Type::giveResource:
					{
						bd.wantToGiveResource = action.intOnly;

					}
					break;
					case action::Type::heroSkillStart:
					{
						heroSkillTrigger = action.intOnly;
					}
					break;
					case action::Type::deltaTimeMultiplier:
					{
						bd.deltaTimeMultiplier = (float)action.intOnly / 100.0f;
					}
					break;
					case action::Type::RandomMove:
					{
						Vector2D v(1, 0);
						v.rotate(RandomRange(0, 360));
						bd.ents[k][i].action.actionDirection = v;
						if (enemySound < enemySoundRandomMove)
						{
							enemySound = enemySoundRandomMove;
						}

					}
					break;
					case action::Type::ShootHero:
					{
						//if (enemySound < enemySoundAttack) { enemySound = enemySoundAttack;  enemySoundChannel = 1; }
						PlaySound(SoundEnum::enemyshoot);
						EntitySummonData esd;
						esd.entityId = 3;
						esd.entityType = 2;
						esd.position = bd.ents[k][i].position;
						esd.summoner = EntityRef(bd.ents[k][i].type, i);
						esd.hp = 1;
						esd.size = 6;
						esd.speedMultiplier = action.shero.shotSpeed / 50;
						bd.entitySummons.push_back(esd);
					}
					break;
					default:
						break;
					}
				}

				bool followingPos = false;
				Vector2D followPos;
				action::MovementData moveData;
#
				if (prog >= 0) //executing actions through the beat
				{
					switch (action.type)
					{
					case action::Type::RandomMove:
						bd.ents[k][i].position += bd.ents[k][i].action.actionDirection * action.rmove.speed *deltaTimeSDL *bd.ents[k][i].speedMultiplier;

						break;
					case action::Type::variableChange:
						VariableGP::variables[action.intArray[0]] = action.intArray[1];
						break;
					case action::Type::sizechange:
						bd.ents[k][i].size += action.vecOnly * deltaTimeSDL;

						break;
					case action::Type::LookAtHero:
						bd.ents[k][i].direction = (bd.Hero().position - bd.ents[k][i].position).normalize();
						bd.ents[k][i].direction.rotate(bd.ents[k][i].rotationOffset);

						break;
					case action::Type::LookAtTarget:
						if (bd.ents[k][i].target.id >= 0)
						{
							bd.ents[k][i].direction = (bd.ents[bd.ents[k][i].target.type][bd.ents[k][i].target.id].position - bd.ents[k][i].position)
								.normalize();
						}

						break;
					case action::Type::pushbackImmunity:
						bd.ents[k][i].pushbackImmunityPriority = action.intOnly;
						break;
					case action::Type::damageTarget:
					{
						if (bd.ents[k][i].target.id >= 0)
						{
							EntityDamage ed;
							ed.damage = action.intOnly;
							ed.entity = bd.ents[k][i].target;
							bd.damages.push_back(ed);
						}

					}
					break;
					case action::Type::pushbackTarget:
					{
						if (bd.ents[k][i].target.id >= 0)
						{
							auto target = &bd.ents[bd.ents[k][i].target.type][bd.ents[k][i].target.id];
							//if (target->pushbackStr < action.intOnly) //override is ok
							{
								target->pushback = (bd.ents[k][i].position*-1 + target->position).normalize();
								target->pushbackStr = action.intOnly;
								target->pushbackPriority = 2;
							}
						}


					}
					break;
					case action::Type::pushbackTargetOnDirection:
					{
						if (bd.ents[k][i].target.id >= 0)
						{
							auto target = &bd.ents[bd.ents[k][i].target.type][bd.ents[k][i].target.id];
							//if (target->pushbackStr < action.intOnly) //override is ok
							{
								target->pushback = bd.ents[k][i].direction;
								target->pushbackStr = action.intOnly;
								target->pushbackPriority = 2;
							}
						}


					}
					break;

					case action::Type::LookAngle:
					{
						auto pos = Vector2D(1, 0);
						pos.rotate(action.intOnly + bd.ents[k][i].rotationOffset);
						bd.ents[k][i].direction = pos;
					}
					break;
					case action::Type::Advance:
						bd.ents[k][i].position += bd.ents[k][i].direction* action.rmove.speed *deltaTimeSDL*bd.ents[k][i].speedMultiplier*speedAdvanceMultiplier;

						break;
					case action::Type::FollowHero:
					{
						followingPos = true;
						followPos = bd.Hero().position;
					}
					break;
					case action::Type::FollowPosition:
					{
						followingPos = true;
						//followPos = action.rmove.position;
					}
					break;
					case action::Type::FollowEnemy:
					{
						followingPos = true;
						for (size_t enemyId = 0; enemyId < bd.ents[1].size(); enemyId++)
						{

							if (bd.ents[1][enemyId].active && bd.ents[1][enemyId].hp > 0) {
								followPos = bd.ents[1][enemyId].position;
								break;
							}
						}
					}
					break;
					case action::Type::FollowParent:
					{
						followingPos = true;
						followPos = bd.ents[bd.ents[k][i].parent.type][bd.ents[k][i].parent.id].position;

					}

					break;
					default:
						break;
					}
				}
				if (followingPos)
				{
					followPos += action.rmove.position;
					Vector2D dis = followPos - bd.ents[k][i].position;
					if (dis.MagnitudeSqr() > action.rmove.minDistanceGetClose*action.rmove.minDistanceGetClose)
					{
						bd.ents[k][i].position = bd.ents[k][i].position + dis.normalize() * action.rmove.speed*deltaTimeSDL*bd.ents[k][i].speedMultiplier;
					}
					else
					{
						if (action.rmove.endWhenReach)
						{
							bd.ents[k][i].action.actionProgress += 99;
						}
					}

				}
				//if (prog >= action.time)
				if (beat2Happen && bd.ents[k][i].action.beatPassAction)
				{
					bd.ents[k][i].action.actionProgress++;
				}
				else
				{
					bd.ents[k][i].action.actionProgress += deltaTimeSDL;
				}

				if (bd.ents[k][i].action.actionProgress >= action.time || action.time <= 0)
				{

					AdvanceAction(bd.ents[k][i]);
				}
				else
				{
					//bd.ents[k][i].action.actionProgress += deltaTimeSDL;
				}

			}


			//bd.ents[k][i] = e;
			//entities[i] = e;
		}

	}
	Profiler::EndTime("entity loop A");

	if (heroSkillTrigger >= 0)
	{
		auto sse = bd.ssEffectDB[(int)BattleEffects::DatabaseEffects::skillActivate];
		sse.pos = bd.Hero().position;
		sse.layer = 8;
		sse.scale = sse.scale * 2;

		bd.ssEffect.push_back(sse);
		bd.Hero().action.actions = bd.heroSkillDatabase[heroSkillTrigger];
		bd.Hero().action.done = false;
		bd.Hero().action.action = 0;
		bd.Hero().action.loopActions = false;
		bd.Hero().action.actionProgress = 0;
	}

	Profiler::StartTime("entity loop");

	{
		/*AddRectangleS({ 5,5, 290, 300 }, color_backgroundB, 0);*/
		AddRectangleS({ 0,305, 300, 300 }, color_backgroundBorder, layerUI);
	}



	renderChoices = false;

	if (bd.state == BattleState::pathChoice)
	{
		renderChoices = true;
		if (BattleChoices::choice == BattleChoices::ChoicesE::easyPath)
		{
			ChangeState(BattleState::afterBattle);
			bd.refusedPaths.push_back(pathIdsSelec[0]);
		}
		if (BattleChoices::choice == BattleChoices::ChoicesE::hardPath)
		{
			bd.wave = 0;
			bd.pathId = pathIdsSelec.at(0);
			ChangeState(BattleState::afterBattle);
		}
	}
	if (bd.state == BattleState::storyChoice)
	{
		renderChoices = true;
		if (BattleChoices::choice == BattleChoices::ChoicesE::KeepPlaying)
		{
			bd.wantStoryUnlockThisBattle = false;
			ChangeState(BattleState::normal);
		}
		if (BattleChoices::choice == BattleChoices::ChoicesE::EndRun)
		{
			return BattleResult::storyWantToUnlock;
		}
	}

	if (bd.state == BattleState::levelUpChoice)
	{
		renderChoices = true;

		if (BattleChoices::choice != BattleChoices::ChoicesE::None)
		{
			int choice = (int)BattleChoices::choice - (int)BattleChoices::ChoicesE::powerUp1;
			auto powerupid = bd.powerUpForChoice[choice];
			auto pu = powerup::powerUpDatabase[powerupid];
			bd.powerUpIdsAcquired.push_back(powerupid);
			bd.powerUpForChoice.clear();

			ApplyPowerUp(pu, powerup::BattleModifierMod());
			ChangeState(BattleState::afterBattle);
		}
	}


	bd.timeOnState += deltaTimeSDLOriginal;
	return BattleResult::noResult;
}

void BattleLoop_Render()
{
	bool tutorialOn = enemyDefeatedAmount_pers[0] == 0;
	if (renderChoices)
	{
		BattleChoices::RenderChoices();
		BattleChoices::Update();
	}


	if (bd.wave <= 1 && bd.state == BattleState::normal)
	{
		auto instruction = "Get close to an enemy to attack\nYou attack automatically";
		TextRender tr(instruction);
		tr.x = 150;
		tr.y = 340;
		//tr.color_Border = { 0,0,0,255 };
		DrawTextSDL(tr, layerUI);
	}

	if (bd.state != BattleState::peace)
	{
		std::string instruction = "Power Level: ";
		instruction.append(std::to_string(bd.heroLevel + 1));
		//instruction.append(std::to_string(bd.heroLevel + 1));
		TextRender tr(instruction.c_str());
		tr.x = 5;
		tr.y = 320;
		tr.justification = -1;
		//tr.color_Border = { 0,0,0,255 };
		DrawTextSDL(tr, 8);

		/*tr.x = 120;
		SDL_itoa(bd.heroLevel + 1, tr.txt, 10);
		DrawTextSDL(tr, 8);*/
		auto size = Vector2D(40, 7);
		uisystem::DrawBar(Vector2D(120 + size.x / 2, tr.y + 8), size, (float)bd.xpAbsorbed / (float)xpReq, 8);
	}

	auto size = Vector2D(40, 7);
	auto yhp = bd.stageHeight + 12;
	std::string s = "HP: ";
	s.append(std::to_string(bd.Hero().hp));
	DrawTextSDL(TextRender(s.c_str(), 5, yhp - 8, { 255,255,255,255 }, 1, -1), 8);
	//DrawTextSDL(TextRender(std::to_string(bd.Hero().hp).c, 5, yhp - 8));
	uisystem::DrawBar(Vector2D(120 + size.x / 2, yhp), size, bd.Hero().hp / (float)bd.Hero().maxHP, 8);


	if (bd.lastEnemyHitId >= 0)
	{
		auto ent = &bd.ents[1][bd.lastEnemyHitId];
		if (ent->machine.state == States::estEnemyAlive)
		{
			auto size = Vector2D(40, 7);
			uisystem::DrawBar(Vector2D(bd.stageWidth - size.x / 2 - 3, size.y + 2), size, ent->hp / (float)ent->maxHP, 6);
		}

	}

	//tutorial #render
	if (tutorialOn && bd.ents[1].size() > 0)
	{
		{
			auto instruction = "Get close to an enemy to attack\nYou attack automatically";
			TextRender tr(instruction);
			tr.x = 150;
			tr.y = enemyTutorialY + 80;
			//tr.color_Border = { 0,0,0,255 };
			DrawTextSDL(tr, 2);
		}
		{
			auto instruction = "Enemy -";
			TextRender tr(instruction);
			tr.x = 130;
			tr.justification = 1;
			tr.y = enemyTutorialY - 5;
			//tr.color_Border = { 0,0,0,255 };
			DrawTextSDL(tr, 2);
		}

	}

	Dialog::Render();

	//entity loop update and rendering #render
	for (size_t k = 0; k < ENTTYPEAMOUNT; k++)
	{
		//auto entities = bd.ents[k];
		for (int i = 0; i < bd.ents[k].size(); i++)
		{
			Entity *ent = &bd.ents[k][i];
			if (ent->active == false) continue;


			//Rendering code #render
			//レンダリング
			Vector2D pos = bd.ents[k][i].position;
			int layer = 2;
			SDL_Color color = color_enemy;
			color = { 255,255,255,255 };
			int sizeInc = 0;

			if (ent->type == 0 && bd.hideHeroTime > 0)
			{
				bd.hideHeroTime -= deltaTimeSDLOriginal;
				continue;
			}

			if (ent->hp <= 0)
			{
				layer = 0;
				color = color_enemyDead;
				if (bd.beatDurationC >= 0) sizeInc += 2;
				if (ent->machine.state == States::estEnemyDeadFollowing)
				{
					//if (bd.beatDurationC >= 0 && heroBeatAttack) color = color_dance;
				}
				//if (ent->machine.state == States::estEnemyDeadA)
				//{
				//	color = color_projharmless;
				//}
				//if (ent->machine.state == States::estEnemyDeadLeaving)
				//{
				//	color = color_projharmless;
				//}
				//if (ent->machine.state == States::estEnemyDeadB)
				//{
				//	color = color_projharmless;
				//	if (bd.beatDurationC >= 0) color = color_enemyDead;
				//}

			}



			if (ent->type == 3)
			{
				color = color_hero;
				int g = color.g;
				int b = color.b;
				color.b = g;
				color.g = b;
				color.b /= 2;
				color.r += 20;
			}




			if (ent->type == 0)
			{
				layer = 4;
				if (bd.beatDurationC >= 0) sizeInc += 2;
				color = color_hero;

				for (size_t i = 0; i < 2; i++)
				{
					auto sizee = ent->size;
					sizee.x += sizeInc + (2 - i) * 2;
					sizee.y += sizeInc + (2 - i) * 2;
					sizeInc -= 0;
					SDL_Rect r_sdl = { (int)(pos.x - sizee.x / 2),(int)pos.y - (int)sizee.y / 2, (int)sizee.x,(int)sizee.y };
					auto color2 = color_damagingA;
					if (i == 1)
						color2 = color_backgroundB;
					if (i == 0)
						color2 = color_hero;
					//AddRectangle(r, layer);
					AddRectangleS(r_sdl, color2, layer);
				}


				const float dangerdis = 55;
				bd.heroDanger = heroMinDisToDamage < 16;
				if (heroMinDisToDamage < dangerdis)
				{
					//color = { 0, 0,255, 1 };

					if (heroMinDisToDamage < 35)
					{
						//color = { 255,0,255,1 };
						//bd.heroBulletTime = 0.3f;
					}
					if (bd.heroDanger)
					{
						//color = { 255,0,0,1 };
						if (bd.heroBulletTimeCDcounter <= 0 && bd.heroBulletTime == 1.0 && bd.Hero().imortal == false)
						{
							bd.heroBulletTime = 0.2f;
							bd.heroBulletTimeMaxCounter = bd.heroBulletTimeMax;
						}

					}
				}
				if (bd.heroBulletTime == 1)
				{
					bd.heroBulletTimeMaxCounter = -1;
				}
				if (ent->hp <= 0)
				{
					sizeInc += 5;
					if (fmod(bd.timeOnState, 0.3f) > 0.15f)
					{
						color = color_enemy;
					}

				}
			}
			if (ent->damaging == true)
			{
				int frameWidth = 4;
				int sizeReduc = 6;
				for (size_t i = 0; i < 2; i++)
				{
					auto sizee = ent->size;
					sizee.x += sizeInc + frameWidth * 3 - i * frameWidth - sizeReduc; //+(2 - i) * 2;
					sizee.y += sizeInc + frameWidth * 3 - i * frameWidth - sizeReduc; //+(2 - i) * 2;

					SDL_Rect r_sdl = { (int)(pos.x - sizee.x / 2),(int)pos.y - (int)sizee.y / 2, (int)sizee.x,(int)sizee.y };
					auto color2 = color_damagingA;


					if (bd.beatDurationC > 0)
					{
						color2 = color_damagingB;
					}
					if (ent->side > 0)
						color2 = color_hero;
					if (i == 1) color2 = bg_color_sdl;
					//AddRectangle(r, layer);
					AddRectangleS(r_sdl, color2, layer);
				}
				sizeInc -= sizeReduc;
				color = bg_color_sdl;

			}
			if (ent->type == 2)
			{
				layer = 3;
				if (bd.beatDurationC > 0 && ent->damaging == false)
				{
					continue;
				}
				color = bg_color_sdl;
				if (ent->damaging == false)
				{
					color = color_projharmless;
				}
			}

			if (ent->type == 1)
			{
				if (false && ent->machine.state == States::estEnemyAlive &&ent->hp < ent->maxHP)
				{
					uisystem::DrawBar(ent->position + Vector2D(0, -18), Vector2D(26, 5), ent->hp / (float)ent->maxHP, 6);
				}
			}

			//stop old border drawing code for enemies
			if (ent->type == 1 && ent->damaging == false && ent->hp > 0 && false)
			{
				for (size_t border = 0; border < 2; border++)
				{
					auto sizee = ent->size;
					sizee.x += sizeInc + (2 - border) * 2;
					sizee.y += sizeInc + (2 - border) * 2;
					sizeInc -= 0;
					SDL_Rect r_sdl = { (int)(pos.x - sizee.x / 2),(int)pos.y - (int)sizee.y / 2, (int)sizee.x,(int)sizee.y };
					auto color2 = color_damagingA;
					if (border == 1)
						color2 = color_backgroundB;
					if (border == 0)
						color2 = color_enemy;
					AddRectangleS(r_sdl, color2, layer);
				}

			}

			if (ent->damageStagInvincible > 0)
			{
				if (bd.beatDurationC >= 0)
				{
					continue;
				}
				color.r = 255;
				color.b = 255;
				color.g /= 255;
			}

			auto sizee = ent->size;
			sizee.x += sizeInc;
			sizee.y += sizeInc;
			SDL_Rect r_sdl = { (int)(pos.x - sizee.x / 2),(int)pos.y - (int)sizee.y / 2, (int)sizee.x,(int)sizee.y };
			//AddRectangle(r, layer);

			if (ent->cooldownValues[0] > 0)
			{
				auto sizee = ent->size;
				ent->cooldownValues[0] -= deltaTimeSDL;
				SDL_Rect r_sdl = { (int)(pos.x - sizee.x / 2),(int)pos.y - (int)sizee.y / 2, (int)sizee.x,(int)sizee.y };
				auto color2 = color_damagingA;

				color2 = color_hero;
				AddRectangleS(r_sdl, color2, layer);
				//color = ;
			}

			if (ent->id < bd.renders[k].size())
			{
				auto render = bd.renders[k][ent->id];
				if (render.text.length() > 0)
				{
					TextRender tr(render.text.c_str(), ent->position.x + ent->size.x, ent->position.y - ent->size.y + 3, { 255,255,255,255 }, 2, -1, -1, 32);

					DrawTextSDL(tr);
				}
				if (render.regions.size() > 0)
				{
					auto reg = render.regions[0];
					reg.x = ent->position.x;
					reg.y = ent->position.y;
					reg.color = color;
					reg.scaleX = (float)(reg.ir.origin.w + sizeInc) / (float)reg.ir.origin.w;
					reg.scaleY = (float)(reg.ir.origin.h + sizeInc) / (float)reg.ir.origin.h;
					if (bd.beatDurationC >= 0) {

						/*reg.scaleX = 1.15f;
						reg.scaleY = 1.15f;*/
					}

					AddRegionDraw(reg, layer);
				}
				else
				{
					AddRectangleS(r_sdl, color, layer);
				}
			}
			else
			{
				AddRectangleS(r_sdl, color, layer);
			}

		}
	}
	for (size_t i = 0; i < bd.ssEffect.size(); i++)
	{
		//auto sse = &bd.ssEffect[i];
		if (!bd.ssEffect[i].active) continue;
		if (bd.ssEffect[i].delay > 0)
		{
			bd.ssEffect[i].delay -= deltaTimeSDLOriginal;
			continue;
		}
		bd.ssEffect[i].counter += deltaTimeSDLOriginal;

		if (bd.ssEffect[i].counter > bd.ssEffect[i].timePerImage)
		{
			bd.ssEffect[i].progress++;
			bd.ssEffect[i].counter = 0;
		}
		if (bd.ssEffect[i].progress < bd.ssEffect[i].regions.size())
		{

			auto r = bd.ssEffect[i].regions[bd.ssEffect[i].progress];
			r.x += bd.ssEffect[i].pos.x;
			r.y += bd.ssEffect[i].pos.y;
			r.scaleX *= bd.ssEffect[i].scale.x;
			r.scaleY *= bd.ssEffect[i].scale.y;
			AddRegionDraw(r, bd.ssEffect[i].layer);
		}
		else
		{
			if (bd.ssEffect[i].loop)bd.ssEffect[i].progress = 0;
			//sse.progress = 0;
		}
	}
	for (size_t i = 0; i < bd.battleTexts.size(); i++)
	{
		bd.battleTexts[i].time += deltaTimeSDLOriginal;
		if (bd.battleTexts[i].time > 22 * 1.0f / 30.0f) continue;
		auto pos = bd.battleTexts[i].pos;
		auto tr = bd.battleTexts[i].tr;
		tr.color_Border = color_backgroundB;
		tr.color = color_hero;
		if (fmod(bd.battleTexts[i].time, 0.4f) < 0.1f) {
			//tr.color = color_dance;
		}
		tr.x += pos.x;
		tr.y += pos.y;
		DrawTextSDL(tr);
	}
	Profiler::EndTime("entity loop");
	//particle render line
	Profiler::StartTime("p_render");
	bd.particleExecuter.Render();
	Profiler::EndTime("p_render");


}

SDL_Rect BattleLoop_HeroRect()
{
	auto posFix = bd.Hero().position - bd.Hero().size*0.5f;

	return { (int)posFix.x, (int)posFix.y, (int)bd.Hero().size.x, (int)bd.Hero().size.y };
}

void BattleLoop_ConstraintHero(SDL_Rect constraint)
{
	BattleLoop_ConstraintPosition(constraint.x, constraint.y, constraint.x + constraint.w, constraint.y + constraint.h, &bd.Hero());
}

void BattleLoop_SetHeroPos(int x, int y)
{
	bd.Hero().position.Set(x, y);
}

void BattleLoop_ChoiceMode()
{
	ChangeState(BattleState::storyChoice);
}

int & BattleLoop_BankedXP()
{
	return bd.totalXP;
	// TODO: insert return statement here
}

int BattleLoop_WaveMain()
{

	return bd.wave;
}

int BattlGetScore()
{
	return bd.score;
}

bool BattleLoop_WantToSave()
{
	return bd.wantToSave;
}

std::vector<int> BattleLoop_GetDataToSave(int saveversion)
{
	std::vector<int> data;
	data.push_back((int)bd.battleTime);
	data.push_back(bd.score);
	data.push_back(bd.clearedWave);
	data.push_back(bd.xpAbsorbed);
	data.push_back(bd.heroLevel);
	data.push_back(bd.enemyFollowLimit);
	data.push_back(efo_followPositions.size());
	if (saveversion >= 6)
	{
		data.push_back(bd.totalXP);
	}
	if (saveversion >= 9)
	{
		data.push_back(bd.pathId);
	}
	data.push_back(bd.powerUpIdsAcquired.size());
	for (size_t i = 0; i < bd.powerUpIdsAcquired.size(); i++)
	{
		data.push_back(bd.powerUpIdsAcquired[i]);
	}



	return data;
}

void BattleLoop_Peace()
{
	StopMusicSDL();
	ChangeState(BattleState::peace);
}

ImageRegion BattleLoop_EnemyMainRegion(int enemyId)
{
	if (enemyId < 0) 
	{
		SDL_Log("enemy id smaller than zero, serious error!");
		enemyId = 0;
	}
		
	/*SDL_Log("enemy id %d", enemyId);
	SDL_Log("region s %d", bd.renders[1].at(enemyId).regions.size());*/
	return bd.renders[1].at(enemyId).regions.at(0).ir;
}

void BattleLoop_HeroCamera(bool active)
{
	bd.heroCamera = active;
}

int BattleLoop_GetEnemySize(int enemyId)
{
	return bd.entitySummonDataSource[1][enemyId].size;
}

void BattleLoop_LoadData(std::vector<int> data, int saveVersion)
{
	float battleTime = 0;
	int i = 0;
	bd.battleTime = data[i]; i++;
	bd.score = data[i]; i++;
	bd.wave = data[i]; i++;
	bd.clearedWave = bd.wave;
	bd.xpAbsorbed = data[i]; i++;
	bd.heroLevel = data[i]; i++;
	if (saveVersion >= 4)
	{
		bd.enemyFollowLimit = data.at(i); i++;
		int currentFollowers = data.at(i); i++;
		for (size_t i = 0; i < currentFollowers; i++)
		{
			EntitySummonData esd;
			esd.entityType = 1;
			esd.entityId = 0;
			esd.hp = -1;
			esd.size = 8;
			bd.entitySummons.push_back(esd);
		}


		//data.push_back(efo_followPositions.size());
	}

	if (saveVersion >= 6) {

		bd.totalXP = data.at(i); i++;
	}

	if (saveVersion >= 9)
	{
		bd.pathId = data.at(i); i++;
	}

	int nPowerUpIds = data.at(i); i++;


	for (size_t p_i = 0; p_i < nPowerUpIds; p_i++)
	{
		bd.powerUpIdsAcquired.push_back(data.at(i)); i++;
	}
	for (size_t i = 0; i < bd.powerUpIdsAcquired.size(); i++)
	{
		if (powerup::powerUpDatabase.size() <= 0) break;
		ApplyPowerUp(powerup::powerUpDatabase[bd.powerUpIdsAcquired[i]], powerup::BattleModifierMod());
	}
}

void BattleLoop_LimitHeroMovement(bool left, bool right, bool down, bool up)
{
	bd.forbiddenLeftDown = Vector2D(0, 0);
	bd.forbiddenUpRight = Vector2D(0, 0);
	if (left)
		bd.forbiddenLeftDown.x = 1;
	if (right)
		bd.forbiddenUpRight.x = 1;
	if (up)
		bd.forbiddenUpRight.y = 1;
	if (down)
		bd.forbiddenLeftDown.y = 1;
}

Entity & BattleData::Hero()
{
	return ents[0][0];
}
