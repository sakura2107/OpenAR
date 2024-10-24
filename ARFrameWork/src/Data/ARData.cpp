#include <Data/ARData.h>
#include <Data/ARJson.h>

ARProject::~ARProject() 
{
	if (size) {
		for (int i = 0; i < size; i++) delete task_array[i];
	}
}

void ARProject::push_back() 
{
	task_array.push_back(new ARTask);
	size++;
}

void ARProject::pop_back() 
{
	if (!size) return;
	delete task_array[size - 1];
	task_array.pop_back();
	size--;
}

void ARProject::insert(const int& loc)
{
	if (loc >= size || loc < 0) return;
	task_array.insert(task_array.begin() + loc, new ARTask);
	size++;
}

void ARProject::erase(const int& loc)
{
	if (loc >= size || loc < 0) return;
	delete task_array[loc];
	task_array.erase(task_array.begin() + loc);
	size--;
}

void ARProject::clear() 
{
	if (!size) return;
	for (int i = 0; i < size; i++) delete task_array[i];
	task_array.clear();
	size = 0;
}

bool ARProject::empty() const
{
	return size == 0;
}

int ARProject::Size() const
{
	return size;
}

ARTask* ARProject::getPtr(const int& loc)
{
	if (loc >= size || loc < 0) return nullptr;
	return task_array[loc];
}

void ARProject::swap(const int& first_loc, const int& second_loc)
{
	if (first_loc >= size || second_loc >= size || first_loc < 0 || second_loc < 0) return;
	ARTask* temp_ptr = task_array[first_loc];
	task_array[first_loc] = task_array[second_loc];
	task_array[second_loc] = temp_ptr;
}

ARTask::~ARTask()
{
	if (size) {
		for (int i = 0; i < size; i++) delete block_array[i];
	}
}

void ARTask::push_back()
{
	block_array.push_back(new ARBlock);
	size++;
}

void ARTask::pop_back()
{
	if (size == 0) return;
	delete block_array[size - 1];
	block_array.pop_back();
	size--;
}

void ARTask::insert(const int& loc)
{
	if (loc >= size || loc < 0) return;
	block_array.insert(block_array.begin() + loc, new ARBlock);
	size++;
}

void ARTask::erase(const int& loc)
{
	if (loc >= size || loc < 0) return;
	delete block_array[loc];
	block_array.erase(block_array.begin() + loc);
	size--;
}

void ARTask::clear()
{
	if (!size) return;
	for (int i = 0; i < size; i++) delete block_array[i];
	block_array.clear();
	size = 0;
}

bool ARTask::empty() const
{
	return size == 0;
}

int ARTask::Size() const
{
	return size;
}

ARBlock* ARTask::getPtr(const int& loc)
{
	if (loc >= size || loc < 0) return nullptr;
	return block_array[loc];
}

void ARTask::swap(const int& first_loc, const int& second_loc)
{
	if (first_loc >= size || second_loc >= size || first_loc < 0 || second_loc < 0) return;
	ARBlock* temp_ptr = block_array[first_loc];
	block_array[first_loc] = block_array[second_loc];
	block_array[second_loc] = temp_ptr;
}

ARBlock::~ARBlock()
{
	if (size) {
		for (int i = 0; i < size; i++) delete code_array[i];
	}
}

void ARBlock::push_back()
{
	code_array.push_back(new ARCode);
	size++;
}

void ARBlock::pop_back()
{
	if (!size) return;
	delete code_array[size - 1];
	code_array.pop_back();
	size--;
}

void ARBlock::insert(const int& loc)
{
	if (loc >= size || loc < 0) return;
	code_array.insert(code_array.begin() + loc, new ARCode);
	size++;
}

void ARBlock::erase(const int& loc)
{
	if (loc >= size || loc < 0) return;
	delete code_array[loc];
	code_array.erase(code_array.begin() + loc);
	size--;
}

void ARBlock::clear()
{
	if (!size) return;
	for (int i = 0; i < size; i++) delete code_array[i];
	code_array.clear();
	size = 0;
}

bool ARBlock::empty() const
{
	return size == 0;
}

int ARBlock::Size() const
{
	return size;
}

ARCode* ARBlock::getPtr(const int& loc)
{
	if (loc >= size || loc < 0) return nullptr;
	return code_array[loc];
}

void ARBlock::swap(const int& first_loc, const int& second_loc)
{
	if (first_loc >= size || second_loc >= size || first_loc < 0 || second_loc < 0) return;
	ARCode* temp_ptr = code_array[first_loc];
	code_array[first_loc] = code_array[second_loc];
	code_array[second_loc] = temp_ptr;
}