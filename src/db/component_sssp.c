#include "graph.h"
#include "schema.h"
#include "tuple.h"
#include "cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <inttypes.h>


/* Place the code for your Dijkstra implementation in this file */


int
component_sssp(
        component_t c,
        vertexid_t v1,
        vertexid_t v2,
        int *n,
        int *total_weight,
        vertexid_t **path)
{

	//helper methods

	//returns an array of the vertexids in the component
	vertexid_t *get_vertexids(component_t c, int num_vertices)
	{

		vertexid_t *vertex_list = malloc(num_vertices * sizeof(vertexid_t));
		int counter = 0;
		off_t off;
		ssize_t len, size;
		vertexid_t id;
		char *buf;
		int readlen;

		if (c->sv == NULL)
			size = 0;
		else
			size = schema_size(c->sv);

		readlen = sizeof(vertexid_t) + size;
		buf = malloc(readlen);

		for (off = 0;; off += readlen)
		{

			lseek(c->vfd, off, SEEK_SET);
			len = read(c->vfd, buf, readlen);
			if (len <= 0)
				break;
			id = *((vertexid_t *)  buf);
			vertex_list[counter] = id;
			counter++;

		}

		free(buf);
		return vertex_list;

	}


	//returns the number of vertices in the component
	int get_num_vertices(component_t c)
	{
		
		int count = 0;
		off_t off;
		vertexid_t id;
		ssize_t len, size;
		char *buf;
		int readlen;

		if (c->sv == NULL)
			size = 0;
		else
			size = schema_size(c->sv);

		readlen = sizeof(vertexid_t) + size;
		buf = malloc(readlen);

		for (off = 0;; off += readlen)
		{

			lseek(c->vfd, off, SEEK_SET);
			len = read(c->vfd, buf, readlen);
			if (len <= 0)
				break;
			id = *((vertexid_t *) buf);
			count++;
	
		}

		free(buf);
		return count;	
		
	}

	//returns 1 if the vertex exists in the component, 0 otherwise
	int does_vertex_exist(component_t c, vertexid_t v)
	{
		for (vertex_t current = c->v; current != NULL; current = current->next)
		{
			if (current->id == v)
				return 1;
		}
		return 0;
	}

	//gets the index of the vertex in the linked list
	int get_vertex_index(component_t c, vertexid_t id)
	{

		int offset = 0;
		
		for (vertex_t current = c->v; current != NULL; current = current->next)
		{
			if (current->id == id)
				return offset;
			offset++;
		}

		return -1;

	}

	int get_offset(component_t c, edge_t e)
	{
		schema_t edgeSchema = c->se;
		attribute_t current_attribute = edgeSchema->attrlist;

		for (attribute_t current_attribute; current_attribute != NULL; current_attribute = current_attribute->next)
		{
			if (current_attribute->bt == INTEGER)
				break;
		}

		if (current_attribute == NULL) 
			return -1;

		return tuple_get_offset(e->tuple, current_attribute->name);
	}

	//returns 1 if there are unvisited vertices, 0 otherwise
	int unvisited(int *visited, int num_vertices)
	{
		for (int x = 0; x <  num_vertices; x++)
		{
			if(!visited[x])
				return 1;
		}
		return 0;
	}
	


	/*********
		Dijkstra
	*********/

	//get number of vertices
	int num_vertices = get_num_vertices(c);

	//check that vertices exist in the component
	if (num_vertices <= 0)
	{
		printf("There are no vertices in this component.\n");
		return -1;
	}
	else if (!does_vertex_exist(c, v1))
	{
		printf("Vertex 1 does not exist in this component.\n");
		return -1;
	}
	else if (!does_vertex_exist(c, v2))
	{
		printf("Vertex 2 does not exist in this component.\n");
		return -1;
	}

	/*********
		initialize data structures for SSSP algorithm
	*********/

	vertexid_t vertices[num_vertices];
	int visited[num_vertices];
	int cost[num_vertices][num_vertices];
	int shortest_distance[num_vertices];
	vertexid_t previous_vertex[num_vertices];

	//fill the 'vertices' array with the vertex ids
	int counter = 0;
	for (vertex_t current = c->v; current != NULL; current = current->next)
	{
		vertices[counter] = current->id;
		counter++;
	}

	//fill 'visited' array with 0's to represent that no vertices have been visited 
	for (int x = 0; x < num_vertices; x++)
	{
		visited[x] = 0;
	}

	//fill 'cost' array with INT_MAX to represent infinity in the SSSP algorithm
	for (int x = 0; x < num_vertices; x++)
	{
		for (int y = 0; y < num_vertices; y++)
		{
			if ( x == y )
				cost[x][y] = 0;
			else
				cost[x][y] = INT_MAX;
		}
	}

	//update the 'cost' array
	for (edge_t current = c->e; current != NULL; current = current->next)
	{
		int x = get_vertex_index(c, current->id1);
		int y = get_vertex_index(c, current->id2);
		cost[x][y] = tuple_get_int(current->tuple->buf + get_offset(c, current));
	}

	//fill 'shortest_distance' array with INT_MAX to represent infinity
	for (int x = 0; x < num_vertices; x++)
		shortest_distance[x] = INT_MAX;

	//fill 'previous_vertex' array with -1
	for (int x = 0; x < num_vertices; x++)
		previous_vertex[x] = -1;

	while (unvisited(visited, num_vertices))
	{
		int index_of_min = -1;
		int min_value = INT_MAX;
		for (int x = 0; x < num_vertices; x++)
		{
			if (shortest_distance[x] <= min_value && !visited[x])
			{
				index_of_min = x;
				min_value = shortest_distance[x];
			}
		}

		if (index_of_min == -1)
			break;

		visited[index_of_min] = 1;

		for (int x = 0; x < num_vertices; x++)
		{
			int connected = 0;
			for (edge_t current = c->e; current != NULL; current = current->next)
			{
				if (current->id1 == vertices[index_of_min] && current->id2 == vertices[x])
					connected = 1;
			}
			if (!visited[x] && connected == 1)
			{
				int new_distance = shortest_distance[index_of_min] + cost[index_of_min][x];
				if (new_distance < shortest_distance[x])
				{
					shortest_distance[x] = new_distance;
					previous_vertex[x] = vertices[index_of_min];
				}
			}
		}
	}

	*n = 0;
	for (vertexid_t current = v2; current != -1; current = previous_vertex[get_vertex_index(c, current)])
	{
		(*n)++;
	}

	*path = malloc(sizeof(vertexid_t) * (*n));
	vertexid_t current_id = v2;
	for (int x = (*n)-1; x >= 0; x--)
	{
		(*path)[x] = current_id;
		current_id = previous_vertex[get_vertex_index(c, current_id)];
	}

	*total_weight = shortest_distance[get_vertex_index(c, v2)];

	return 0;
}
