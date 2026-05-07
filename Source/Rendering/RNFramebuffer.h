//
//  RNFramebuffer.h
//  Rayne
//
//  Copyright 2015 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//


#ifndef __RAYNE_FRAMEBUFFER_H_
#define __RAYNE_FRAMEBUFFER_H_

#include "../Base/RNBase.h"
#include "../Objects/RNObject.h"
#include "RNTexture.h"

namespace RN
{
	class Framebuffer : public Object
	{
	public:
		struct TargetView
		{
			static TargetView WithTexture(Texture *texture, uint16 mipmap = 0, uint16 slice = 0, uint16 length = 0)
			{
				TargetView targetView;
				targetView.texture = texture;
				targetView.mipmap = mipmap;
				targetView.slice = slice;
				targetView.length = length;
				return targetView;
			}

			uint16 GetResolvedLength() const
			{
				if(length != 0 || !texture)
					return length;

				const Texture::Descriptor &descriptor = texture->GetDescriptor();
				switch(descriptor.type)
				{
					case Texture::Type::Type1DArray:
					case Texture::Type::Type2DArray:
					case Texture::Type::TypeCube:
					case Texture::Type::TypeCubeArray:
					case Texture::Type::Type3D:
						RN_ASSERT(slice < descriptor.depth, "Target view slice exceeds texture layer count");
						return static_cast<uint16>(descriptor.depth - slice);

					default:
						return 1;
				}
			}

			TargetView GetResolvedTargetView() const
			{
				TargetView targetView = *this;
				targetView.length = GetResolvedLength();
				return targetView;
			}

			Texture *texture;
			uint16 mipmap;
			uint16 slice;
			uint16 length;
		};

		const Vector2 &GetSize() const { return _size; }
		RNAPI Vector2 GetRenderAreaSize() const;
		RNAPI virtual void SetSize(Vector2 size);
		RNAPI void SetRenderAreaSize(Vector2 size);

		RNAPI virtual uint32 GetColorTargetCount() const = 0;
		RNAPI virtual Texture *GetColorTexture(uint32 index = 0) const = 0;
		RNAPI virtual Texture *GetDepthStencilTexture() const = 0;
		RNAPI virtual uint8 GetSampleCount() const = 0;

		RNAPI virtual void SetColorTarget(const TargetView &target, uint32 index = 0) = 0;
		RNAPI virtual void SetDepthStencilTarget(const TargetView &target) = 0;

	protected:
		RNAPI Framebuffer(const Vector2 &size);
		RNAPI ~Framebuffer();

		Vector2 _size;
		Vector2 _renderAreaSize;

	private:
		void ClampRenderAreaSize();

		__RNDeclareMetaInternal(Framebuffer)
	};
} // namespace RN


#endif /* __RAYNE_FRAMEBUFFER_H_ */
