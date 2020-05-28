#pragma once
#include "BattleLoop.h"
#include "main.h"
#include "uicode.h"

namespace SpiritXPScreen 
{

	array<std::vector<Vector2D>, 20> crystalPositions;
	int state = 0;
	bool pause = false;
	bool skip = false;
	int spiritThatLevelUp = -1;
	int totalXP = 0;

	void Reset() 
	{
		pause = false;
		skip = false;
	}

	int Draw(bool wannaAdvance) 
	{
		if (!pause && wannaAdvance) 
		{
			skip = true;
		}
		if (pause && wannaAdvance) 
		{
			pause = false;
		}

		{
			uisystem::DrawTextWithBackground("Spirit XP", 10);
		}
		
		auto crystalImage = AtlasManager::GetImage("musicnote2");
		bool summonCrystal = false;
		bool trigger = periodicTriggerSDL(0.2f);
		float bfx = totalXP;
		if (bfx > 0 && trigger && !pause) 
		{
			totalXP = totalXP - 1;
			summonCrystal = true;
		}
		if (totalXP <= 0 &&wannaAdvance) 
		{
			bool over= true;
			for (size_t i = 0; i < crystalPositions.size(); i++)
			{
				if (crystalPositions[i].size() != 0) 
				{
					over = false;
					break;
				}
			}
			if (over) {
				return -1;
			}
		}
		
		Vector2D initialCrystalPos(widthSDL/2, 40);
		int spiritPos = 0;
		if (pause)
		{
			auto sp = powerup::joinedSpirits[powerup::activeSpirits[spiritThatLevelUp]];
			uisystem::DrawTextWithBackground("LEVEL UP", 40);
			uisystem::DrawSpiritInfo(sp, 80 + 30, 1);
		}
		else 
		{

			for (size_t i = 0; i < powerup::activeSpirits.size(); i++)
			{
				if (powerup::activeSpirits[i] >= 0)
				{
					int enemyId = powerup::joinedSpirits[powerup::activeSpirits[i]].enemyId;
					auto reg = BattleLoop_EnemyMainRegion(enemyId);
					ImageRegionDraw ird;
					ird.ir = reg;
					int maxPerRow = 5;
					ird.x = (spiritPos%maxPerRow) * 40 + 32;
					ird.y = spiritPos / maxPerRow * 60 + 100;
					ird.color = color_enemyDead;
					AddRegionDraw(ird, layerUI);
					auto sp = powerup::joinedSpirits[powerup::activeSpirits[i]];
					float xpLevel = (float)powerup::XPForLevel(sp);
					uisystem::DrawBar(Vector2D(ird.x, (int)ird.y + 36), Vector2D(36, 8),
						(float)sp.xp / xpLevel, layerUI);
					if (summonCrystal)
					{
						crystalPositions[i].push_back(initialCrystalPos);
					}
					if (skip)
					{
						powerup::joinedSpirits[powerup::activeSpirits[i]].xp += totalXP;
					}

					if (!pause)
					{
						for (size_t c = 0; c < crystalPositions[i].size(); c++)
						{
							auto dis = crystalPositions[i][c] - Vector2D(ird.x, ird.y);
							crystalPositions[i][c] = crystalPositions[i][c] + dis.normalize() * -220 * deltaTimeSDL;
							ImageRegionDraw irdC;
							irdC.x = crystalPositions[i][c].x;
							irdC.y = crystalPositions[i][c].y;
							irdC.ir = crystalImage;
							AddRegionDraw(irdC, layerUI);
							if (dis.MagnitudeSqr() < 5)
							{
								crystalPositions[i].erase(crystalPositions[i].begin() + c);
								c--;
								powerup::joinedSpirits[powerup::activeSpirits[i]].xp++;

							}
						}
						if (powerup::joinedSpirits[powerup::activeSpirits[i]].xp >= xpLevel)
						{
							powerup::joinedSpirits[powerup::activeSpirits[i]].xp = powerup::joinedSpirits[powerup::activeSpirits[i]].xp - xpLevel;
							powerup::joinedSpirits[powerup::activeSpirits[i]].level++;
							pause = true;
							spiritThatLevelUp = i;
							break;
						}
					}


					spiritPos++;
				}


			}

		}
		if(skip)
			totalXP = 0;
	}
}