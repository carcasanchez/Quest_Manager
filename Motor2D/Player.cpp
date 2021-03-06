#include "Player.h"
#include "j1App.h"
#include "j1Input.h"
#include "j1FileSystem.h"
#include "j1Textures.h"
#include "j1Render.h"
#include "j1Map.h"
#include "j1Pathfinding.h"
#include "j1CollisionManager.h"
#include "p2Log.h"
#include "Animation.h"

Player::Player() : Entity() {} 

bool Player::Spawn(std::string file, iPoint pos)
{
	bool ret = true;

	string tmp = file;

	// set position
	currentPos = lastPos = pos;

	// load xml attributes
	pugi::xml_document	attributesFile;
	char* buff;
	int size = App->fs->Load(file.c_str(), &buff);
	pugi::xml_parse_result result = attributesFile.load_buffer(buff, size);
	RELEASE(buff);

	if (result == NULL)
	{
		LOG("Could not load attributes xml file. Pugi error: %s", result.description());
		ret = false;
	}
	else
	{
		pugi::xml_node attributes = attributesFile.child("attributes");

		// base stats
		pugi::xml_node tmp = attributes.child("base");
		maxLife = life = tmp.attribute("life").as_int(1);
		maxStamina = stamina = tmp.attribute("stamina").as_int(100);
		staminaRec = tmp.attribute("staminaRec").as_float();
		speed = tmp.attribute("speed").as_int(70);

		// attack
		tmp = attributes.child("attack");
		attackSpeed = tmp.attribute("speed").as_int(40);
		attackTax = tmp.attribute("staminaTax").as_int(20);

		//dodge
		tmp = attributes.child("dodge");
		dodgeSpeed = tmp.attribute("speed").as_int(500);
		dodgeTax = tmp.attribute("staminaTax").as_int(25);
		dodgeLimit = tmp.attribute("limit").as_int(50);

		//collider
		tmp = attributes.child("collider");
		colPivot = { tmp.attribute("x").as_int(8), tmp.attribute("y").as_int(12) };
		col = App->collisions->AddCollider({ pos.x, pos.y, tmp.attribute("w").as_int(16), tmp.attribute("h").as_int(15) }, COLLIDER_PLAYER, ((j1Module*)App->game));
	}

	if (ret)
	{
		if (ret = loadAnimations())
		{
			SDL_Texture* playerTex = App->tex->Load("textures/Link_sprites/Link_young.png");
			currentAnim = &anim.find({ IDLE, D_DOWN })->second;
			sprite = new Sprite(playerTex, currentPos, SCENE, currentAnim->getCurrentFrame(), currentAnim->pivot);
			actionState = IDLE;
		}
	}

	return ret;
}

bool Player::Update(float dt)
{
	bool ret = true;
	lastPos = currentPos;

	if (stamina < maxStamina)
	{
		stamina += staminaRec;
	}
	else stamina = maxStamina;

	switch (actionState)
	{
		case(IDLE):
			Idle();
			break;

		case(WALKING):
			Walking(dt);
			break;

		case(ATTACKING):
			Attacking(dt);
			break;

		case(DODGING):
			Dodging(dt);
			break;
	}

	UpdateCollider();
	return ret;
}

bool Player::loadAnimations()
{
	bool ret = true;

	pugi::xml_document	anim_file;
	pugi::xml_node		animation;
	char* buff;
	int size = App->fs->Load("animations/player_animations.xml", &buff);
	pugi::xml_parse_result result = anim_file.load_buffer(buff, size);
	RELEASE(buff);

	if (result == NULL)
	{
		LOG("Could not load animation xml file. Pugi error: %s", result.description());
		ret = false;
	}
	else
		animation = anim_file.child("animations");

	if (ret == true)
	{
		pugi::xml_node ent = animation.child("LINK");

		for (pugi::xml_node action = ent.child("IDLE"); action != NULL; action = action.next_sibling())
		{
			for (pugi::xml_node dir = action.child("UP"); dir != action.child("loop"); dir = dir.next_sibling())
			{
				std::pair<ACTION_STATE, DIRECTION> p;
				int state = action.child("name").attribute("value").as_int();
				p.first = (ACTION_STATE)state;

				int di = dir.first_child().attribute("name").as_int();
				p.second = (DIRECTION)di;

				Animation anims;
				int x = dir.first_child().attribute("x").as_int();
				int y = dir.first_child().attribute("y").as_int();
				int w = dir.first_child().attribute("w").as_int();
				int h = dir.first_child().attribute("h").as_int();
				int fN = dir.first_child().attribute("frameNumber").as_int();
				int margin = dir.first_child().attribute("margin").as_int();
				bool loop = action.child("loop").attribute("value").as_bool();
				int pivotX = dir.first_child().attribute("pivot_x").as_int();
				int pivotY = dir.first_child().attribute("pivot_y").as_int();
				int flip = dir.first_child().attribute("flip").as_int();
				float animSpeed = action.child("speed").attribute("value").as_float();

				anims.setAnimation(x, y, w, h, fN, margin);
				anims.loop = loop;
			
				anims.speed = animSpeed;
				anims.pivot.x = pivotX;
				anims.pivot.y = pivotY;

				if (flip == 1)
					anims.flip = SDL_FLIP_HORIZONTAL;
				else if (flip == 2)
					anims.flip = SDL_FLIP_VERTICAL;
				else anims.flip = SDL_FLIP_NONE;


				iPoint piv;

				anim.insert(std::pair<std::pair<ACTION_STATE, DIRECTION>, Animation >(p, anims));
				anim.find({ p.first, p.second })->second.pivot.Set(pivotX, pivotY);
				piv = anim.find({ p.first, p.second })->second.pivot;
			}
		}
	}


	return ret;
}

void Player::Change_direction()
{
	if (App->input->GetKey(SDL_SCANCODE_UP) == KEY_REPEAT)
		currentDir = D_UP;
	if (App->input->GetKey(SDL_SCANCODE_DOWN) == KEY_REPEAT)
		currentDir = D_DOWN;
	if (App->input->GetKey(SDL_SCANCODE_RIGHT) == KEY_REPEAT)
		currentDir = D_RIGHT;
	if (App->input->GetKey(SDL_SCANCODE_LEFT) == KEY_REPEAT)
		currentDir = D_LEFT;
}

//Displace the entity a given X and Y taking in account collisions w/map
void Player::Move(int x, int y)
{
	currentPos.x += x;
	UpdateCollider();
	if(col->CheckMapCollision())
		currentPos.x -= x;

	currentPos.y += y;
	UpdateCollider();
	if (col->CheckMapCollision())
		currentPos.y -= y;
}

void Player::UpdateCollider()
{
	col->rect.x = currentPos.x - colPivot.x;
	col->rect.y = currentPos.y - colPivot.y;
}

bool Player::Idle()
{
	Change_direction();

	if (App->input->GetKey(SDL_SCANCODE_W) == KEY_REPEAT || App->input->GetKey(SDL_SCANCODE_A) == KEY_REPEAT || App->input->GetKey(SDL_SCANCODE_S) == KEY_REPEAT || App->input->GetKey(SDL_SCANCODE_D) == KEY_REPEAT)
	{
		actionState = WALKING;
		return true;
	}

	if (App->input->GetKey(SDL_SCANCODE_SPACE) == KEY_DOWN && (stamina - attackTax >= 0))
	{
		stamina -= attackTax;
		Change_direction();
		actionState = ATTACKING;
		return true;
	}

	return false;
}

bool Player::Walking(float dt)
{
	bool moving = false;
	dodgeDir = { 0, 0 };

	if (App->input->GetKey(SDL_SCANCODE_S) == KEY_REPEAT)
	{
		currentDir = D_DOWN;
		dodgeDir.y = 1;
		Move(0, SDL_ceil(speed * dt));
		moving = true;
	}
	else if (App->input->GetKey(SDL_SCANCODE_W) == KEY_REPEAT)
	{
		currentDir = D_UP;
		dodgeDir.y = -1;
		Move(0, -SDL_ceil(speed * dt));
		moving = true;
	}

	if (App->input->GetKey(SDL_SCANCODE_A) == KEY_REPEAT)
	{
		currentDir = D_LEFT;
		dodgeDir.x = -1;
		Move(-SDL_ceil(speed * dt), 0);
		moving = true;

	}
	else if (App->input->GetKey(SDL_SCANCODE_D) == KEY_REPEAT)
	{
		currentDir = D_RIGHT;
		dodgeDir.x = 1;
		Move(SDL_ceil(speed * dt), 0);
		moving = true;
	}

	if (moving == false)
	{
		actionState = IDLE;
		return true;
	}

	if (App->input->GetKey(SDL_SCANCODE_C) == KEY_DOWN && (stamina- dodgeTax >=0))
	{	
		stamina -= dodgeTax;
		Change_direction();
		actionState = DODGING;
		dodgeTimer.Start();
		return true;
	}
	 
	if (App->input->GetKey(SDL_SCANCODE_SPACE) == KEY_DOWN && (stamina - attackTax >= 0))
	{
 		stamina -= attackTax;
		Change_direction();
		actionState = ATTACKING;
		return true;
	}

	Change_direction();

	return false;
}

bool Player::Attacking(float dt)
{
	if (App->input->GetKey(SDL_SCANCODE_S) == KEY_REPEAT)
	{
		Move(0, SDL_ceil(attackSpeed * dt));
	}
	else if (App->input->GetKey(SDL_SCANCODE_W) == KEY_REPEAT)
	{
		Move(0, -SDL_ceil(attackSpeed * dt));
	}

	if (App->input->GetKey(SDL_SCANCODE_A) == KEY_REPEAT)
	{
		Move(-SDL_ceil(attackSpeed * dt), 0);
	}
	else if (App->input->GetKey(SDL_SCANCODE_D) == KEY_REPEAT)
	{
		Move(SDL_ceil(attackSpeed * dt), 0);
	}

	if (currentAnim->isOver())
	{
		currentAnim->Reset();
		actionState = IDLE;
	}

	return true;
}

bool Player::Dodging(float dt)
{
	if (dodgeTimer.ReadMs() > dodgeLimit)
	{
		actionState = IDLE;
	}

	Move(SDL_ceil(dodgeSpeed * dodgeDir.x * dt), SDL_ceil(dodgeSpeed*dodgeDir.y* dt));
	
	return true;
}