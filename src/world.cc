/*
 *  Stage : a multi-robot simulator.
 *  Copyright (C) 2001, 2002 Richard Vaughan, Andrew Howard and Brian Gerkey.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
/*
 * Desc: A world device model - replaces the CWorld class
 * Author: Richard Vaughan
 * Date: 31 Jan 2003
 * CVS info: $Id: world.cc,v 1.149 2003-10-12 19:30:32 rtv Exp $
 */


#include <math.h>
#include <stdio.h>
#include <assert.h>

//#define DEBUG

#include "stage.h"
#include "world.hh"
#include "matrix.hh"

// declare a function from main.cc for use here
// write a stg_property_t on the channel returns TRUE on success, else FALSE
gboolean StgPropertyWrite( GIOChannel* channel, stg_property_t* prop );


stg_world_t* stg_world_create( stg_client_data_t* client, 
			       stg_id_t id,
			       stg_world_create_t* rc ) 
{
  stg_world_t* world = (stg_world_t*)calloc( sizeof(stg_world_t), 1 );
  assert( world );

  // add this world to the client's list
  client->worlds = g_list_append( client->worlds, world ); 
  
  // must set the id, name and token before using the BASE_DEBUG macro
  world->id = id; // a unique id for this object
  world->name = g_string_new( rc->name );
  WORLD_DEBUG( world, "construction" );
  
  
  world->client = client; // this client created this world
  world->width = rc->width;
  world->height = rc->height;
  world->ppm = 1.0/rc->resolution;
  world->node = NULL; // we attach a model tree here
  world->running = FALSE;  
  world->win = NULL; // gui data is attached here
  
  // create a root node that will contain my model tree
  world->node = g_node_new( world );
  
  WORLD_DEBUG( world, "world construction complete" );

  WORLD_DEBUG( world,"world startup");
  
  g_assert( stg_world_create_matrix( world ) );

  if( world->win ) PRINT_WARN( "creating window, but window pointer not NULL" );  
  world->win = stg_gui_window_create( world, 0,0 ); // use default dimensions

  world->running = TRUE;
  
  WORLD_DEBUG( world, "world startup complete" );

  // console output
  PRINT_MSG2( "Created world \"%s\" (%d).", world->name->str, world->id ); 

  world->interval = 0.1; 
  world->clock_tag = g_timeout_add( (int)(world->interval * 1000.0), 
				    stg_world_clock_tick, world );
  
  return world;
}



void stg_update_entity( GNode* node, void* data )
{
  // recursively update the children of this node
  g_node_children_foreach( node, G_TRAVERSE_ALL,
			   stg_update_entity, data );
  
  // update this node
  CEntity* ent = (CEntity*)node->data;
  stg_world_t* world = (stg_world_t*)data;

  if( (world->time - ent->last_update) > (double)ent->interval / 1000.0 )
    ent->Update();
}

void stg_world_send_time( gpointer data, gpointer userdata )
{
  puts( "sending time" );
  stg_client_data_t* subscriber = (stg_client_data_t*)data;
  stg_world_t* world = (stg_world_t*)userdata;


  printf( "client %p df %d\n", subscriber, g_io_channel_unix_get_fd(subscriber->channel) );

  int a = 5;

  stg_property_t* prop = stg_property_create();

  prop->id = 6;
  prop->property = (stg_prop_id_t)8;

  StgPropertyWrite( subscriber->channel, prop );

  stg_property_free( prop );
}


gboolean stg_world_clock_tick( void* data )
{
  
  stg_world_t* world = (stg_world_t*)data;
  world->time += world->interval;
  //printf( " time: %.3f   \n", world->time );

  // update my entities 
  g_node_children_foreach( world->node, G_TRAVERSE_ALL,
			   stg_update_entity, world );
 
  puts( "foreach subscriber" );
  //send the time to all our subscribers
  g_list_foreach( world->subscribers, stg_world_send_time, world );

  //usleep( 5000 );

  return TRUE; // keep calling me
}

int stg_world_destroy( stg_world_t* world )
{
  WORLD_DEBUG( world, "world destruction" );

  // stop the clock
  if( world->clock_tag )
    g_source_remove( world->clock_tag );

  // recursively shutdown all the entities in the world
  while( stg_world_first_child(world) )
    delete stg_world_first_child(world);
  

  // kill this gui window
  if( world->win ) stg_gui_window_destroy( world->win );
  world->win = NULL;
  
  world->running = FALSE;
  WORLD_DEBUG( world,"world shutdown complete");
  
  // remove the world from the client that created it
  world->client->worlds = g_list_remove( world->client->worlds, world ); 

  if( world->matrix ) stg_world_destroy_matrix( world );

  WORLD_DEBUG( world, "world destruction complete" );

  PRINT_MSG1( "Destroyed world \"%s\".", world->name->str );

  free(world);
  return 0; //ok
}


CMatrix* stg_world_create_matrix( stg_world_t* world )
{
  g_assert( world );
  if( world->matrix ) stg_world_destroy_matrix( world );
  
  WORLD_DEBUG3( world, "Creating a matrix [%.2fx%.2f]m at %.2f ppm",
		world->width, world->height, world->ppm );
  g_assert( (world->matrix = 
	     new CMatrix( world->width, world->height, world->ppm, 1) ) );
  
  return world->matrix;
}

void stg_world_destroy_matrix( stg_world_t* world )
{
  g_assert( world );
  if( world->matrix == NULL )
    PRINT_WARN( "destroy matrix called for NULL matrix pointer!" );
  else
    delete world->matrix;
  
  world->matrix = NULL;
}
  
