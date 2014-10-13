#pragma once

#include <emergent/Emergent.h>
#include <emergent/image/Colours.hpp>
#include <emergent/struct/Buffer.hpp>
#include <emergent/struct/Distribution.hpp>
// #include <emergent/Maths.h>

// #include <stdexcept>
// #include <typeinfo>
// #include <functional>
#include <FreeImage.h>


namespace emergent
{
	/// Base class for images of a given arithmetic type.
	template <class T = byte> class ImageBase
	{
		static_assert(std::is_arithmetic<T>::value, "Image type must be numeric or boolean");

		public:
			/// Destructor
			virtual ~ImageBase() { }

			/// Operator override to support implicit and explicit typecasting
			operator T*() { return this->buffer; }

			/// Return the buffer data
			T *Data() { return this->buffer; }

			/// Return the actual buffer.
			/// WARNING: If you modify the size of this buffer you will corrupt the owner image
			Buffer<T> *Internal() { return &this->buffer; }

			/// Return the image type ID
			int Type() { return this->type; }

			/// Return the image depth (in bytes)
			int Depth() { return this->depth; }

			/// Return the image width
			int Width() { return this->width; }

			/// Return the image height
			int Height() { return this->height; }

			/// Return the image size
			int Size() { return this->width * this->height; }

			/// Clear the image - sets all pixel values to 0 regardless of type
			void Clear() { this->buffer.Clear(); }


			/// Resizes the image buffers but destroys any existing image data.
			void Resize(int width, int height)
			{
				if (width > 0 && height > 0)
				{
					this->width		= width;
					this->height	= height;
					this->buffer.Resize(width * height * this->depth);
				}
				this->lookup.Resize(0);
			}


			/// Returns the maximum value in the current image data (regardless of image depth).
			T Max()
			{
				T result 	= std::numeric_limits<T>::min();
				T *src		= this->buffer;
				int size	= this->buffer.Size();

				for (int i=0; i<size; i++, src++) if (*src > result) result = *src;

				return result;
			}


			/// Returns the minimum value in the current image data (regardless of image depth).
			T Min()
			{
				T result 	= std::numeric_limits<T>::max();
				T *src		= this->buffer;
				int size	= this->buffer.Size();

				for (int i=0; i<size; i++, src++) if (*src < result) result = *src;

				return result;
			}


			/// Truncate the height of the image down to the given height. Since this just requires
			/// a change of the height value and a cut of the buffer it allows you to chop the
			/// bottom off of an image without needing to use an additional image buffer.
			bool Truncate(int height)
			{
				if (height < this->height && height > 0)
				{
					if (this->buffer.Truncate(height * this->width * this->depth))
					{
						this->height = height;
						return true;
					}
				}

				return false;
			}


			/// Count the number of values in the current image data (regardless of image depth)
			/// that match the supplied predicate.
			int Count(std::function<bool(T value)> predicate)
			{
				int result	= 0;
				int size	= this->buffer.Size();
				T *src		= this->buffer;

				for (int i=0; i<size; i++) if (predicate(*src++)) result++;

				return result;
			}


			/// Count the number of zero values in the current image data (regardless of image depth)
			int ZeroCount()
			{
				int result	= 0;
				int size	= this->buffer.Size();
				T *src		= this->buffer;

				for (int i=0; i<size; i++) if (!*src++) result++;

				return result;
			}

			///Check if all the pixels in the image are set to the same value (regardless of image depth)
			bool IsBlank(T reference = 0)
			{
				T* src 		= this->buffer;
				int size 	= this->buffer.Size();
				for (int i = 0; i < size; i++)
				{
					if(*src++ != reference) return false;
				}
				return true;
			}


			/// Clamp the current image data (regardless of image depth) to the supplied
			/// lower (black) and upper (white) limits.
			void Clamp(T black, T white)
			{
				T *src 		= this->buffer;
				int size	= this->width * this->height * this->depth;

				for(int i = 0; i< size; i++, src++)
				{
					*src = *src < black ? black : (*src > white ? white : *src);
				}
			}


			/// Load an image from file. Uses the freeimage library to attempt loading
			/// and conversion of the image. It should cope with any standard image
			/// formats.
			bool Load(std::string path)
			{
				bool result = false;
				auto fif	= FreeImage_GetFileType(path.c_str(), 0);

				if (fif == FIF_UNKNOWN) fif = FreeImage_GetFIFFromFilename(path.c_str());

				if (fif != FIF_UNKNOWN && FreeImage_FIFSupportsReading(fif))
				{
					auto *image = FreeImage_Load(fif, path.c_str());

					if (image)
					{
						result = this->FromFib(image);
						FreeImage_Unload(image);
					}
				}

				return result;
			}


			/// Load an image from memory buffer
			bool Load(Buffer<byte> &buffer)
			{
				bool result	= false;
				auto *mem 	= FreeImage_OpenMemory(buffer.Data(), buffer.Size());

				if (mem)
				{
					auto fif = FreeImage_GetFileTypeFromMemory(mem, 0);

					if (fif != FIF_UNKNOWN && FreeImage_FIFSupportsReading(fif))
					{
						auto *image = FreeImage_LoadFromMemory(fif, mem);

						if (image)
						{
							result = this->FromFib(image);
							FreeImage_Unload(image);
						}
					}

					FreeImage_CloseMemory(mem);
				}

				return result;
			}


			/// Save an image to file. Uses the freeimage library to attempt saving
			/// out the image (converts it to byte first but does not scale the values,
			/// so be warned). Image format is automatically determined by file extension.
			bool Save(std::string path, int compression = 0)
			{
				bool result = false;

				if (this->Size())
				{
					auto *image = this->ToFib();

					if (image)
					{
						auto fif = FreeImage_GetFIFFromFilename(path.c_str());

						if (fif != FIF_UNKNOWN && FreeImage_FIFSupportsWriting(fif))
						{
							switch (compression)
							{
								case 0:		result = FreeImage_Save(fif, image, path.c_str(), PNG_DEFAULT);			break;
								case 1:		result = FreeImage_Save(fif, image, path.c_str(), JPEG_QUALITYGOOD);	break;
								default:	result = FreeImage_Save(fif, image, path.c_str(), JPEG_QUALITYNORMAL);	break;
							}
						}

						FreeImage_Unload(image);
					}
				}

				return result;
			}



			/// Save image to a memory buffer.
			bool Save(Buffer<byte> &buffer, int compression)
			{
				bool result = false;

				if (this->Size())
				{
					DWORD size;
					byte *data;
					auto *mem 	= FreeImage_OpenMemory();
					auto *image = this->ToFib();

					if (image)
					{
						switch (compression)
						{
							case 0:		result = FreeImage_SaveToMemory(FIF_PNG, image, mem, PNG_DEFAULT);			break;
							case 1:		result = FreeImage_SaveToMemory(FIF_JPEG, image, mem, JPEG_QUALITYGOOD);	break;
							default:	result = FreeImage_SaveToMemory(FIF_JPEG, image, mem, JPEG_QUALITYNORMAL);	break;
						}

						FreeImage_Unload(image);
					}

					if (result && FreeImage_AcquireMemory(mem, &data, &size))
					{
						buffer.Set(data, size);
					}
					else result = false;

					FreeImage_CloseMemory(mem);
				}

				return result;
			}


			/// Calculate the distribution statistics of the image
			distribution Stats()
			{
				return distribution(this->buffer);
			}


			/// Threshold this image at the given value (each channel is thresholded independently)
			void Threshold(T value, bool above = true)
			{
				int size	= this->width * this->height * this->depth;
				T *src		= this->buffer;

				if (above)	for (int i=0; i<size; i++, src++) 	*src = (*src < value) ? 0 : 255;
				else		for (int i=0; i<size; i++, src++)	*src = (*src > value) ? 0 : 255;
			}


			/// Shift all of the values in the image data (regardless of image depth) by the
			/// specified amount.
			void Shift(int value)
			{
				int size	= this->width * this->height * this->depth;
				T *src		= this->buffer;

				for (int i=0; i<size; i++, src++) *src = Maths::clamp<T>(*src + value);
			}


			/// Gets the value of a pixel for a specific channel but supports values outside the image dimensions,
			/// if mirror is true the pixel values are mirrored, otherwise they are smeared. Weird things will happen
			/// if a value that is twice the width/height outside the image is requested.
			T Value(int x, int y, byte channel = 0, bool mirror = true)
			{
				if (channel < this->depth)
				{
					int ax 		= x < 0 ? (mirror ? -x : 0) : x < this->width  ? x : (mirror ? this->width + this->width - x - 2 : this->width - 1);
					int ay 		= y < 0 ? (mirror ? -y : 0) : y < this->height ? y : (mirror ? this->height + this->height - y - 2 : this->height - 1);
					int line	= this->width * this->depth;

					return *(this->buffer + ay * line + ax * this->depth + channel);
				}

				return 0;
			}


			/// Interpolate the pixel value of a specific channel at the given coordinates.
			T Interpolate(double x, double y, byte channel = 0)
			{
				if (x >= 0 && x < this->width - 1 && y >= 0 && y < this->height - 1 && channel < this->depth)
				{
					int line	= this->width * this->depth;
					double sx	= x - (int)x;
					double sy	= y - (int)y;
					T *pb		= this->buffer + (int)y * line + (int)x * this->depth + channel;

					return lrint(
						(double)*pb * (1 - sx) * (1 - sy) +
						(double)*(pb + this->depth) * sx * (1 - sy) +
						(double)*(pb + line) * (1 - sx) * sy +
						(double)*(pb + line + this->depth) * sx * sy
					);
				}

				return 0;
			}


			/// Variance normalise the image to a target variance.
			/// Each channel is normalised independently.
			bool VarianceNormalise(double targetVariance)
			{
				int size = this->Size();

				if (size)
				{
					T *b;
					int i, j;
					double mean, variance;
					double sum[Depth::max] = {}, squared[Depth::max] = {}, scale[Depth::max], shift[Depth::max];

					for (i=0, b=buffer; i<size; i++)
					{
						for (j=0; j<this->depth; j++)
						{
							squared[j]	+= *b * *b;
							sum[j]		+= *b++;
						}
					}

					for (i=0; i<this->depth; i++)
					{
						mean		= sum[i] / (double)size;
						variance	= (squared[i] / (double)size) - mean * mean;
						scale[i]	= sqrt(targetVariance / variance);
						shift[i]	= 128 - (mean * scale[i]);
					}

					for (i=0, b=buffer; i<size; i++)
					{
						for (j=0; j<this->depth; j++, b++)
						{
							*b = Maths::clamp<T>(lrint((double)*b * scale[j] + shift[j]));
						}
					}

					return true;
				}

				return false;
			}


			/// Normalise the image data. Each channel is normalised
			/// independently.
			bool Normalise()
			{
				int size = this->Size();

				if (size)
				{
					T *b;
					int i, j;
					double scale[Depth::max];
					T low[Depth::max], high[Depth::max];

					std::fill_n(low, Depth::max, std::numeric_limits<T>::max());
					std::fill_n(high, Depth::max, std::numeric_limits<T>::min());

					for (i=0, b=buffer; i<size; i++)
					{
						for (j=0; j<this->depth; j++)
						{
							low[j]	= std::min(low[j], *b);
							high[j]	= std::max(high[j], *b++);
						}
					}

					for (i=0; i<this->depth; i++)
					{
						scale[i] = 255.0 / std::max(1.0, (double)(high[i] - low[i]));
					}

					for (i=0, b=buffer; i<size; i++)
					{
						for (j=0; j<this->depth; j++, b++)
						{
							*b = lrint(scale[j] * (*b - low[j]));
						}
					}

					return true;
				}

				return false;
			}


			/// Generate a row lookup table
			T **Lookup()
			{
				if (this->lookup.Size() != this->height)
				{
					this->lookup.Resize(this->height);
					int line	= this->width * this->depth;
					T *pb		= this->buffer;
					T **pl		= this->lookup;

					for (int y=0; y<this->height; y++, pb+=line, pl++) *pl = pb;
				}

				return this->lookup;
			}


		protected:
			ImageBase(int type, int depth, int width = 0, int height = 0)
				: type(type), depth(depth), width(width), height(height), buffer(width * height * depth), lookup() {}

			/// Image type
			int type;

			/// Image depth
			int depth;

			/// Image width
			int width;

			/// Image height
			int height;

			/// Buffer for the actual image data
			Buffer<T> buffer;

			/// Lookup table for rows
			Buffer<T*> lookup;

		private:

			inline void SWAPBYTES(T& a, T& b) { a ^= b; b ^= a; a ^= b; }

			FIBITMAP *ToFib()
			{
				auto *result = FreeImage_ConvertFromRawBits(this->buffer, this->width, this->height, this->width * this->depth, this->depth * 8, 0, 0, 0, true);

				#if FREEIMAGE_COLORORDER == FREEIMAGE_COLORORDER_BGR
					if (result && this->depth > 1)
					{
						int x, y;
						byte *data	= FreeImage_GetBits(result);
						int jump	= FreeImage_GetPitch(result) - this->width * this->depth;

						for (y=0; y<this->height; y++, data += jump)
						{
							for (x=0; x<this->width; x++, data += this->depth)
							{
								SWAPBYTES(data[0], data[2]);
							}
						}
					}
				#endif

				return result;
			}


			bool FromFib(FIBITMAP *image)
			{
				this->width 	= FreeImage_GetWidth(image);
				this->height	= FreeImage_GetHeight(image);

				this->buffer.Resize(this->width * this->height * this->depth);

				FreeImage_ConvertToRawBits(this->buffer, image, this->width * this->depth, this->depth * 8, 0, 0, 0, true);

				#if FREEIMAGE_COLORORDER == FREEIMAGE_COLORORDER_BGR
					if (this->depth > 1)
					{
						byte *end = this->buffer + this->width * this->height * this->depth;

						for (byte *data = this->buffer; data < end; data += this->depth) SWAPBYTES(data[0], data[2]);
					}
				#endif

				return true;
			}
	};
}