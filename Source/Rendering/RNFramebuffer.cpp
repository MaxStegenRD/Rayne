//
//  RNFramebuffer.cpp
//  Rayne
//
//  Copyright 2015 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNFramebuffer.h"
#include "RNRenderer.h"

namespace RN
{
	RNDefineMeta(Framebuffer, Object)

	Framebuffer::Framebuffer(const Vector2 &size) :
		_size(size),
		_renderAreaSize(size)
	{}

	Framebuffer::~Framebuffer()
	{}

	Vector2 Framebuffer::GetRenderAreaSize() const
	{
		if(_renderAreaSize.x < 0.5f || _renderAreaSize.y < 0.5f)
			return _size;

		return _renderAreaSize;
	}

	void Framebuffer::SetSize(Vector2 size)
	{
		_size = size;
		ClampRenderAreaSize();
	}

	void Framebuffer::SetRenderAreaSize(Vector2 size)
	{
		_renderAreaSize = size;
		ClampRenderAreaSize();
	}

	void Framebuffer::ClampRenderAreaSize()
	{
		if(_renderAreaSize.x > _size.x) _renderAreaSize.x = _size.x;
		if(_renderAreaSize.y > _size.y) _renderAreaSize.y = _size.y;
	}
} // namespace RN
