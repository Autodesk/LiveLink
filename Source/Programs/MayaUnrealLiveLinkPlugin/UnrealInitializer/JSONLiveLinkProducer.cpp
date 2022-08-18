// MIT License

// Copyright (c) 2022 Autodesk, Inc.

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "JSONLiveLinkProducer.h"

#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Misc/FileHelper.h"

#include "Roles/LiveLinkAnimationRole.h"
#include "Roles/LiveLinkAnimationTypes.h"
#include "Roles/LiveLinkCameraRole.h"
#include "Roles/LiveLinkCameraTypes.h"
#include "Roles/LiveLinkLightRole.h"
#include "Roles/LiveLinkLightTypes.h"
#include "Roles/LiveLinkTransformRole.h"
#include "Roles/LiveLinkTransformTypes.h"

static const int SEND_BUFFER_SIZE  = 1024 * 1024;

#ifdef RAPIDJSON_VERSION_STRING
#else
	// Conversion table to JSon from Unreal.
	#define StartObject WriteObjectStart
	#define EndObject   WriteObjectEnd
	#define StartArray  WriteArrayStart
	#define EndArray    WriteArrayEnd
	#define Double      WriteValue
	#define String      WriteValue
	#define Key         WriteIdentifierPrefix
#endif

int ResolveHelper(const char* Hostname, int Family, const char* Service, sockaddr_storage* Addr)
{
	if (Addr == nullptr)
	{
		return -1;
	}

	addrinfo* Result_list = NULL;
	addrinfo Hints = {};
	Hints.ai_family = Family;
	Hints.ai_socktype = SOCK_DGRAM;
	int Result = getaddrinfo(Hostname, Service, &Hints, &Result_list);
	if (Result == 0)
	{
#if PLATFORM_WINDOWS
    	memcpy_s(Addr, sizeof(sockaddr_storage), Result_list->ai_addr, Result_list->ai_addrlen);
#else
    	memcpy(Addr, Result_list->ai_addr, sizeof(sockaddr_storage));
#endif		
		freeaddrinfo(Result_list);
	}

	return Result;
}

FJSONLiveLinkProducer::FJSONLiveLinkProducer(const FString& ProviderName)
: Socket(0)
, Writer(StringBuffer)
{
}

FJSONLiveLinkProducer::~FJSONLiveLinkProducer()
{
	CloseConnection();
}

bool FJSONLiveLinkProducer::Connect(const FIPv4Endpoint& InEndpoint)
{
	CloseConnection();

	Socket = socket(AF_INET, SOCK_DGRAM, 0);
	FString Error("Resolve error: ");

	sockaddr_in AddrListen = {}; // zero-int, sin_port is 0, which picks a random port for bind.
	AddrListen.sin_family = AF_INET;
	int Result = bind(Socket, (sockaddr*)&AddrListen, sizeof(AddrListen));
	if (Result == -1)
	{
		int Lasterror = errno;
		
		Error.AppendInt(Lasterror);
		FPlatformMisc::LowLevelOutputDebugString(*Error);
		CloseConnection();
		return false;
	}

	FString AddressName = InEndpoint.Address.ToString();
	FString PortName = FString::FromInt(InEndpoint.Port);
	Result = ResolveHelper(TCHAR_TO_ANSI(*AddressName), AF_INET, TCHAR_TO_ANSI(*PortName), &AddrDest);
	if (Result != 0)
	{
		int Lasterror = errno;

		Error.AppendInt(Lasterror);
		FPlatformMisc::LowLevelOutputDebugString(*Error);
		CloseConnection();
		return false;
	}

	SendSuccess = true;
	return true;
}

void FJSONLiveLinkProducer::CloseConnection()
{
	OnConnectionStatusChanged.Clear();
	if (Socket)
	{
#if PLATFORM_WINDOWS
		closesocket(Socket);
#else
		close(Socket);
#endif
	}
	AddrDest = {};
	Socket = 0;
	SendSuccess = false;
	ResetWriter();
}

void FJSONLiveLinkProducer::ResetWriter()
{
#ifdef RAPIDJSON_VERSION_STRING
	StringBuffer.Clear();
#else
	StringBuffer.Reset();
#endif
	Writer.Reset(StringBuffer);
}

bool FJSONLiveLinkProducer::SendStringBuffer()
{
#ifdef RAPIDJSON_VERSION_STRING
	if (!FileExport)
	{
		const int32 Size = StringBuffer.GetSize();
		if (Size <= SEND_BUFFER_SIZE)
		{
			SendSuccess = sendto(Socket, StringBuffer.GetString(), Size, 0, (sockaddr*)&AddrDest, sizeof(AddrDest)) > 0;
			return SendSuccess;
		}
	}
	else
	{
		FString JSONString(StringBuffer.GetString());
		return FFileHelper::SaveStringToFile(JSONString, *FileExportPath);
	}
#else
	if (!FileExport)
	{
		const int32 Size = StringBuffer.Num();
		if (Size <= SEND_BUFFER_SIZE)
		{
			SendSuccess = sendto(Socket, (const char *)StringBuffer.GetData(), Size, 0, (sockaddr*)&AddrDest, sizeof(AddrDest)) > 0;
			return SendSuccess;
		}
	}
	else
	{
		FString JSONString;
		for (int32 i = 0; i < StringBuffer.Num(); i+=sizeof(TCHAR))
		{
			TCHAR* Char = static_cast<TCHAR*>(static_cast<void*>(&StringBuffer[i]));
			JSONString += *Char;
		}
		return FFileHelper::SaveStringToFile(JSONString, *FileExportPath);
	}
#endif
	return false;
}

/**
* Send, to UE4, the static data of a subject.
* @param SubjectName	The name of the subject
* @param Role			The Live Link role of the subject. The StaticData type should match the role's data.
* @param StaticData		The valid static data of the subject. See FUnrealStreamManager::InitializeAndGetStaticData.
The FLiveLinkStaticDataStruct doesn't have a copy constructor.
*						The argument is passed by r-value to help the user understand the compiler error message.
* @return				True if the message was sent or is pending an active connection.
* @see					UpdateSubjectFrameData, RemoveSubject
*/
bool FJSONLiveLinkProducer::UpdateSubjectStaticData(const FName& SubjectName, TSubclassOf<ULiveLinkRole> Role, FLiveLinkStaticDataStruct&& StaticData)
{
	if (Socket == 0)
	{
		return false;
	}

	auto RoleClass = *Role;
	bool SupportedRole = true;
	if (RoleClass == ULiveLinkAnimationRole::StaticClass())
	{
		UpdateSubjectStaticData(SubjectName, *StaticData.Cast<FLiveLinkSkeletonStaticData>());
	}
	else if (RoleClass == ULiveLinkCameraRole::StaticClass())
	{
		UpdateSubjectStaticData(SubjectName, *StaticData.Cast<FLiveLinkCameraStaticData>());
	}
	else if (RoleClass == ULiveLinkLightRole::StaticClass())
	{
		UpdateSubjectStaticData(SubjectName, *StaticData.Cast<FLiveLinkLightStaticData>());
	}
	else if (RoleClass == ULiveLinkTransformRole::StaticClass())
	{
		UpdateSubjectStaticData(SubjectName, *StaticData.Cast<FLiveLinkTransformStaticData>());
	}
	else
	{
		SupportedRole = false;
	}

	if (SupportedRole)
	{
		SetLastSubjectStaticData(SubjectName, RoleClass, MoveTemp(StaticData));
	}

	return true;
}

/**
* Inform UE4 that a subject won't be streamed anymore.
* @param SubjectName	The name of the subject.
*/
void FJSONLiveLinkProducer::RemoveSubject(const FName& SubjectName)
{
	if (Socket == 0)
	{
		return;
	}

	ResetWriter();

	Writer.StartObject();
	// Subject
#ifdef RAPIDJSON_VERSION_STRING
	Writer.Key(TCHAR_TO_UTF8(*SubjectName.ToString()));
	Writer.String("Remove");
#else
	Writer.WriteValue(SubjectName.ToString(), FString("Remove"));
#endif	
	Writer.EndObject();

	SendStringBuffer();

	ClearTrackedSubject(SubjectName);
}

bool FJSONLiveLinkProducer::UpdateSubjectFrameData(const FName& SubjectName, TSubclassOf<ULiveLinkRole> Role, FLiveLinkFrameDataStruct&& FrameData)
{
	if (Socket == 0)
	{
		return false;
	}

	auto RoleClass = *Role;
	bool SupportedRole = true;
	if (RoleClass == ULiveLinkAnimationRole::StaticClass())
	{
		UpdateSubjectFrameData(SubjectName, *FrameData.Cast<FLiveLinkAnimationFrameData>());
	}
	else if (RoleClass == ULiveLinkCameraRole::StaticClass())
	{
		UpdateSubjectFrameData(SubjectName, *FrameData.Cast<FLiveLinkCameraFrameData>());
	}
	else if (RoleClass == ULiveLinkLightRole::StaticClass())
	{
		UpdateSubjectFrameData(SubjectName, *FrameData.Cast<FLiveLinkLightFrameData>());
	}
	else if (RoleClass == ULiveLinkTransformRole::StaticClass())
	{
		UpdateSubjectFrameData(SubjectName, *FrameData.Cast<FLiveLinkTransformFrameData>());
	}
	else
	{
		SupportedRole = false;
	}

	return SupportedRole;
}

bool FJSONLiveLinkProducer::HasConnection() const
{
	return Socket != 0;
}

/** Function for managing connection status changed delegate. */
FDelegateHandle FJSONLiveLinkProducer::RegisterConnStatusChangedHandle(const FLiveLinkProviderConnectionStatusChanged::FDelegate& ConnStatusChanged) 
{
	return OnConnectionStatusChanged.Add(ConnStatusChanged);
}

/** Function for managing connection status changed delegate. */
void FJSONLiveLinkProducer::UnregisterConnStatusChangedHandle(FDelegateHandle Handle) 
{
	OnConnectionStatusChanged.Remove(Handle);
}

// Get the cached data for the named subject
template<typename T>
T* FJSONLiveLinkProducer::GetLastSubjectStaticData(const FName& SubjectName)
{
	auto SubjectStaticData = StaticDatas.FindByKey(SubjectName);
	if (SubjectStaticData == nullptr)
	{
		return nullptr;
	}

	return SubjectStaticData->StaticData.Cast<T>();
}

void FJSONLiveLinkProducer::SetLastSubjectStaticData(const FName& SubjectName, TSubclassOf<ULiveLinkRole> Role, FLiveLinkStaticDataStruct&& StaticData)
{
	FTrackedStaticData* Result = StaticDatas.FindByKey(SubjectName);
	if (Result)
	{
		Result->StaticData = MoveTemp(StaticData);
		Result->RoleClass = Role.Get();
	}
	else
	{
		StaticDatas.Emplace(SubjectName, Role.Get(), MoveTemp(StaticData));
	}
}

void FJSONLiveLinkProducer::EnableFileExport(bool Enable,
											 const FString& FilePath)
{
	FileExport = Enable;
	FileExportPath = FilePath;
}

void FJSONLiveLinkProducer::ClearTrackedSubject(const FName& SubjectName)
{
	const int32 StaticIndex = StaticDatas.IndexOfByKey(SubjectName);
	if (StaticIndex != INDEX_NONE)
	{
		StaticDatas.RemoveAtSwap(StaticIndex);
	}
}

void FJSONLiveLinkProducer::UpdateSubjectStaticData(const FName& SubjectName,
													FLiveLinkSkeletonStaticData& StaticData)
{
	const auto NumBones = StaticData.BoneNames.Num();
	if (NumBones > 0 &&
		NumBones == StaticData.BoneParents.Num())
	{
		ResetWriter();

		StartWriterStaticData(SubjectName, "Anim");

		Writer.StartObject();
		Writer.Key("BoneHierarchy");
		Writer.StartArray();
		for (int i = 0; i < NumBones; ++i)
		{
			Writer.StartObject();
			WriteKey("Name", true, StaticData.BoneNames[i].ToString());
			WriteKey("Parent", true, StaticData.BoneParents[i]);
			Writer.EndObject();
		}
		Writer.EndArray();
		Writer.EndObject();

		if (StaticData.PropertyNames.Num() > 0)
		{
			Writer.StartObject();
			WriteKey("Properties", true, StaticData.PropertyNames);
			Writer.EndObject();
		}

		EndWriterStaticData();

		SendStringBuffer();
	}
}

void FJSONLiveLinkProducer::UpdateSubjectFrameData(const FName& SubjectName, 
												   const FLiveLinkAnimationFrameData& FrameData)
{
	auto StaticDataPtr = GetLastSubjectStaticData<FLiveLinkSkeletonStaticData>(SubjectName);
	if (StaticDataPtr == nullptr)
	{
		return;
	}
	auto& StaticData = *StaticDataPtr;

	auto NumBones = FrameData.Transforms.Num();
	if (NumBones > 0)
	{
		ResetWriter();

		double SceneTimeInSeconds = FrameData.MetaData.SceneTime.AsSeconds();
		StartWriterFrameData(SubjectName, "Anim", SceneTimeInSeconds);

		// Bones
		Writer.StartObject();
		Writer.Key("BoneTransforms");
		Writer.StartArray();
		for (int i = 0; i < NumBones; ++i)
		{
			Writer.StartObject();
			WriteTransform(FrameData.Transforms[i]);
			Writer.EndObject();
		}
		Writer.EndArray();
		Writer.EndObject();

		if (FrameData.PropertyValues.Num() > 0)
		{
			Writer.StartObject();
			WriteKey("Properties", true, FrameData.PropertyValues);
			Writer.EndObject();
		}

		EndWriterFrameData();

		SendStringBuffer();
	}
}

void FJSONLiveLinkProducer::UpdateSubjectStaticData(const FName& SubjectName,
													FLiveLinkCameraStaticData& StaticData)
{
	ResetWriter();

	StartWriterStaticData(SubjectName, "Camera");

	Writer.StartObject();

	WriteTransformStaticDataKeys(StaticData);

	WriteKey("FieldOfView", StaticData.bIsFieldOfViewSupported, StaticData.bIsFieldOfViewSupported);
	WriteKey("AspectRatio", StaticData.bIsAspectRatioSupported, StaticData.bIsAspectRatioSupported);
	WriteKey("FocalLength", StaticData.bIsFocalLengthSupported, StaticData.bIsFocalLengthSupported);
	WriteKey("ProjectionMode", StaticData.bIsProjectionModeSupported, StaticData.bIsProjectionModeSupported);
	WriteKey("FilmBackWidth", StaticData.FilmBackWidth > 0.0f, StaticData.FilmBackWidth);
	WriteKey("FilmBackHeight", StaticData.FilmBackHeight > 0.0f, StaticData.FilmBackHeight);
	WriteKey("Aperture", StaticData.bIsApertureSupported, StaticData.bIsApertureSupported);
	WriteKey("FocusDistance", StaticData.bIsFocusDistanceSupported, StaticData.bIsFocusDistanceSupported);

	Writer.EndObject();

	EndWriterStaticData();

	SendStringBuffer();
}

void FJSONLiveLinkProducer::UpdateSubjectFrameData(const FName& SubjectName,
												   const FLiveLinkCameraFrameData& FrameData)
{
	auto StaticDataPtr = GetLastSubjectStaticData<FLiveLinkCameraStaticData>(SubjectName);
	if (StaticDataPtr == nullptr)
	{
		return;
	}
	FLiveLinkCameraStaticData& StaticData = *StaticDataPtr;

	ResetWriter();

	double SceneTimeInSeconds = FrameData.MetaData.SceneTime.AsSeconds();
	StartWriterFrameData(SubjectName, "Camera", SceneTimeInSeconds);

	Writer.StartObject();

	WriteTransformFrameDataKeys(FrameData, StaticData);

	WriteKey("FieldOfView", StaticData.bIsFieldOfViewSupported, FrameData.FieldOfView);
	WriteKey("AspectRatio", StaticData.bIsAspectRatioSupported, FrameData.AspectRatio);
	WriteKey("FocalLength", StaticData.bIsFocalLengthSupported, FrameData.FocalLength);
	// Writing orthographic mode only if projection mode is supported
	// Otherwise, don't write anything and assume it's perspective mode
	const bool isOrtho = FrameData.ProjectionMode == ELiveLinkCameraProjectionMode::Orthographic;
	WriteKey("Ortho",
			 StaticData.bIsProjectionModeSupported && isOrtho,
			 isOrtho);
	WriteKey("Aperture", StaticData.bIsApertureSupported, FrameData.Aperture);
	WriteKey("FocusDistance", StaticData.bIsFocusDistanceSupported, FrameData.FocusDistance);

	Writer.EndObject();

	EndWriterFrameData();

	SendStringBuffer();
}

void FJSONLiveLinkProducer::UpdateSubjectStaticData(const FName& SubjectName,
													FLiveLinkLightStaticData& StaticData)
{
	ResetWriter();

	StartWriterStaticData(SubjectName, "Light");

	Writer.StartObject();

	WriteTransformStaticDataKeys(StaticData);

	WriteKey("Temperature", StaticData.bIsTemperatureSupported, StaticData.bIsTemperatureSupported);
	WriteKey("Intensity", StaticData.bIsIntensitySupported, StaticData.bIsIntensitySupported);
	WriteKey("LightColor", StaticData.bIsLightColorSupported, StaticData.bIsLightColorSupported);
	WriteKey("InnerConeAngle", StaticData.bIsInnerConeAngleSupported, StaticData.bIsInnerConeAngleSupported);
	WriteKey("OuterConeAngle", StaticData.bIsOuterConeAngleSupported, StaticData.bIsOuterConeAngleSupported);
	WriteKey("AttenuationRadius", StaticData.bIsAttenuationRadiusSupported, StaticData.bIsAttenuationRadiusSupported);
	WriteKey("SourceLength", StaticData.bIsSourceLenghtSupported, StaticData.bIsSourceLenghtSupported);
	WriteKey("SourceRadius", StaticData.bIsSourceRadiusSupported, StaticData.bIsSourceRadiusSupported);
	WriteKey("SoftSourceRadius", StaticData.bIsSoftSourceRadiusSupported, StaticData.bIsSoftSourceRadiusSupported);

	Writer.EndObject();

	EndWriterStaticData();

	SendStringBuffer();
}

void FJSONLiveLinkProducer::UpdateSubjectFrameData(const FName& SubjectName,
												   const FLiveLinkLightFrameData& FrameData)
{
	auto StaticDataPtr = GetLastSubjectStaticData<FLiveLinkLightStaticData>(SubjectName);
	if (StaticDataPtr == nullptr)
	{
		return;
	}
	FLiveLinkLightStaticData& StaticData = *StaticDataPtr;

	ResetWriter();

	double SceneTimeInSeconds = FrameData.MetaData.SceneTime.AsSeconds();
	StartWriterFrameData(SubjectName, "Light", SceneTimeInSeconds);

	Writer.StartObject();

	WriteTransformFrameDataKeys(FrameData, StaticData);

	WriteKey("Temperature", StaticData.bIsTemperatureSupported, FrameData.Temperature);
	WriteKey("Intensity", StaticData.bIsIntensitySupported, FrameData.Intensity);
	WriteKey("LightColor", StaticData.bIsLightColorSupported, FrameData.LightColor);
	WriteKey("InnerConeAngle", StaticData.bIsInnerConeAngleSupported, FrameData.InnerConeAngle);
	WriteKey("OuterConeAngle", StaticData.bIsOuterConeAngleSupported, FrameData.OuterConeAngle);
	WriteKey("AttenuationRadius", StaticData.bIsAttenuationRadiusSupported, FrameData.AttenuationRadius);
	WriteKey("SourceLength", StaticData.bIsSourceLenghtSupported, FrameData.SourceLength);
	WriteKey("SourceRadius", StaticData.bIsSourceRadiusSupported, FrameData.SourceRadius);
	WriteKey("SoftSourceRadius", StaticData.bIsSoftSourceRadiusSupported, FrameData.SoftSourceRadius);

	Writer.EndObject();

	EndWriterFrameData();

	SendStringBuffer();
}

void FJSONLiveLinkProducer::UpdateSubjectStaticData(const FName& SubjectName, FLiveLinkTransformStaticData& StaticData)
{
	ResetWriter();

	StartWriterStaticData(SubjectName, "Transf");

	Writer.StartObject();

	WriteTransformStaticDataKeys(StaticData);

	Writer.EndObject();

	EndWriterStaticData();

	SendStringBuffer();
}

void FJSONLiveLinkProducer::UpdateSubjectFrameData(const FName& SubjectName,
												   const FLiveLinkTransformFrameData& FrameData)
{
	auto StaticDataPtr = GetLastSubjectStaticData<FLiveLinkTransformStaticData>(SubjectName);
	if (StaticDataPtr == nullptr)
	{
		return;
	}
	FLiveLinkTransformStaticData& StaticData = *StaticDataPtr;

	ResetWriter();

	double SceneTimeInSeconds = FrameData.MetaData.SceneTime.AsSeconds();
	StartWriterFrameData(SubjectName, "Transf", SceneTimeInSeconds);

	Writer.StartObject();

	WriteTransformFrameDataKeys(FrameData, StaticData);

	Writer.EndObject();

	EndWriterFrameData();

	SendStringBuffer();
}

void FJSONLiveLinkProducer::WriteKey(const char* KeyName,
									 bool Supported,
									 bool Value)
{
	if (Supported)
	{
#ifdef RAPIDJSON_VERSION_STRING
		Writer.Key(KeyName);
		Writer.Bool(Value);
#else
		Writer.WriteValue(KeyName,Value);
#endif
	}
}

void FJSONLiveLinkProducer::WriteKey(const char* KeyName,
									 bool Supported,
									 int Value)
{
	if (Supported)
	{
#ifdef RAPIDJSON_VERSION_STRING
		Writer.Key(KeyName);
		Writer.Int(Value);
#else
		Writer.WriteValue(KeyName,Value);
#endif
	}
}

void FJSONLiveLinkProducer::WriteKey(const char* KeyName,
									 bool Supported,
									 double Value)
{
	if (Supported)
	{
#ifdef RAPIDJSON_VERSION_STRING
		Writer.Key(KeyName);
		Writer.Double(Value);
#else
		Writer.WriteValue(KeyName,Value);
#endif
	}
}

void FJSONLiveLinkProducer::WriteKey(const char* KeyName,
									 bool Supported,
									 const FString& Value)
{
	if (Supported)
	{
#ifdef RAPIDJSON_VERSION_STRING
		Writer.Key(KeyName);
		Writer.String(TCHAR_TO_UTF8(*Value));
#else
		Writer.WriteValue(KeyName,Value);
#endif
	}
}

void FJSONLiveLinkProducer::WriteKey(const char* KeyName,
									 bool Supported,
									 const FVector& Value)
{
	if (Supported)
	{
		Writer.Key(KeyName);
		Writer.StartArray();
		Writer.Double(Value.X);
		Writer.Double(Value.Y);
		Writer.Double(Value.Z);
		Writer.EndArray();
	}
}

void FJSONLiveLinkProducer::WriteKey(const char* KeyName,
									 bool Supported,
									 const FQuat& Value)
{
	if (Supported)
	{
		Writer.Key(KeyName);
		Writer.StartArray();
		Writer.Double(Value.X);
		Writer.Double(Value.Y);
		Writer.Double(Value.Z);
		Writer.Double(Value.W);
		Writer.EndArray();
	}
}

void FJSONLiveLinkProducer::WriteKey(const char* KeyName,
									 bool Supported,
									 const FColor& Value)
{
	if (Supported)
	{
		Writer.Key(KeyName);
		Writer.StartArray();
		Writer.Double(Value.R);
		Writer.Double(Value.G);
		Writer.Double(Value.B);
		Writer.Double(Value.A);
		Writer.EndArray();
	}
}

void FJSONLiveLinkProducer::WriteKey(const char* KeyName,
									 bool Supported,
									 const TArray<FName>& Value)
{
	if (Supported)
	{
		Writer.Key(KeyName);
		Writer.StartArray();
		for (const auto& Name : Value)
		{
#ifdef RAPIDJSON_VERSION_STRING
			Writer.String(TCHAR_TO_UTF8(*Name.ToString()));
#else
			Writer.WriteValue(Name.ToString());
#endif
		}
		Writer.EndArray();
	}
}

void FJSONLiveLinkProducer::WriteKey(const char* KeyName,
									 bool Supported,
									 const TArray<float>& Value)
{
	if (Supported)
	{
		Writer.Key(KeyName);
		Writer.StartArray();
		for (const auto& Number : Value)
		{
			Writer.Double(Number);
		}
		Writer.EndArray();
	}
}

void FJSONLiveLinkProducer::WriteLocation(const FVector& Location,
										  bool isLocationSupported)
{
	WriteKey("L", isLocationSupported, Location);
}

void FJSONLiveLinkProducer::WriteRotation(const FQuat& Rotation,
										  bool isRotationSupported)
{
	WriteKey("R", isRotationSupported, Rotation);
}

void FJSONLiveLinkProducer::WriteScale(const FVector& Scale,
									   bool isScaleSupported)
{
	WriteKey("S", isScaleSupported, Scale);
}

void FJSONLiveLinkProducer::WriteTransform(const FTransform& Transform,
										   bool isLocationSupported,
										   bool isRotationSupported,
										   bool isScaleSupported)
{
	WriteLocation(Transform.GetLocation(), isLocationSupported);
	WriteRotation(Transform.GetRotation(), isRotationSupported);
	WriteScale(Transform.GetScale3D(), isScaleSupported);
}

void FJSONLiveLinkProducer::WriteTransform(const FTransform& Transform,
										   const FLiveLinkTransformStaticData& StaticData)
{
	WriteTransform(Transform, StaticData.bIsLocationSupported, StaticData.bIsRotationSupported, StaticData.bIsScaleSupported);
}

void FJSONLiveLinkProducer::WriteTransformStaticDataKeys(const FLiveLinkTransformStaticData& StaticData)
{
	WriteKey("Location", StaticData.bIsLocationSupported, StaticData.bIsLocationSupported);
	WriteKey("Rotation", StaticData.bIsRotationSupported, StaticData.bIsRotationSupported);
	WriteKey("Scale", StaticData.bIsScaleSupported, StaticData.bIsScaleSupported);
}

void FJSONLiveLinkProducer::WriteTransformFrameDataKeys(const FLiveLinkTransformFrameData& FrameData,
														const FLiveLinkTransformStaticData& StaticData)
{
	WriteTransform(FrameData.Transform, StaticData);
}

void FJSONLiveLinkProducer::StartWriterStaticData(const FName& SubjectName,
												  const char* RoleName)
{
	Writer.StartObject();

	// Subject
#ifdef RAPIDJSON_VERSION_STRING
	Writer.Key(TCHAR_TO_UTF8(*SubjectName.ToString()));
#else
	Writer.WriteIdentifierPrefix(SubjectName.ToString());
#endif
	Writer.StartArray();

	// Role
	Writer.StartObject();
#ifdef RAPIDJSON_VERSION_STRING
	Writer.Key("Role");
	Writer.String(RoleName);
#else
	Writer.WriteValue("Role",FString(RoleName));
#endif
	Writer.EndObject();
}

void FJSONLiveLinkProducer::EndWriterStaticData()
{
	Writer.EndArray();
	Writer.EndObject();
}

void FJSONLiveLinkProducer::StartWriterFrameData(const FName& SubjectName,
												 const char* RoleName,
												 double SceneTimeInSeconds)
{
	StartWriterStaticData(SubjectName, RoleName);
	// Time
	Writer.StartObject();
#ifdef RAPIDJSON_VERSION_STRING
	Writer.Key("Time");
	Writer.Double(SceneTimeInSeconds);
#else
	Writer.WriteValue("Time",SceneTimeInSeconds);
#endif
	Writer.EndObject();
}

void FJSONLiveLinkProducer::EndWriterFrameData()
{
	EndWriterStaticData();
}
