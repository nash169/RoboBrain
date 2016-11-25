/*
 *  receiver.h
 *
 *  This file is part of RoboBrain.
 *  Copyright (C) 2016 Bernardo Fichera
 *
 *  RoboBrain is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  RoboBrain is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with RoboBrain.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef RECEIVER_H
#define RECEIVER_H

#include <music.hh>
#include <vector>

//Create a son of the class MUSIC::EventHandlerGLobalIndex to receive spikes from network
class Receiver : public MUSIC::EventHandlerGlobalIndex
{
public:

	Receiver(int *neurons, int num_pops);

	Receiver();

	virtual ~Receiver();

	void operator () (double t, MUSIC::GlobalIndex id);

	std::vector <std::vector <double> >* GetSpikes(int pop);

	std::vector < std::vector < std::vector<double> > > storage;

protected:

private:
	int new_id, pop;
	std::vector <int> bound;
};

#endif // RECEIVER_H