#pragma once

#include "Net/HeadersContainer.h"

namespace Net {
;
struct ConnectData;
class Request;

class Reply : public wxEvtHandler
{
	NO_COPY_CLASS(Reply);

public:
	Reply(const Request& request, asio::ip::tcp::socket* takedSocket, asio::io_service* takedIOService);
	virtual ~Reply();

	const Request& GetRequest() const;
	size_t GetStatuseCode() const;

	const HeadersContainer& GetHeaders() const;
	HeadersContainer& GetHeaders();
	
	const wxString GetError() const;
	boost::optional<wxURL> GetRedirectUrl() const;
	wxString GetFilename() const;
	wxUint64 GetResourceSize() const;

	size_t ReadData(size_t readCount = UINT_MAX);
	size_t ReadDataUntil(const wxString& pattern);
	
	const wxMBConv& GetConverter() const;
	size_t GetReceiveBufferSize() const;

	bool IsReadFinished() const;
	wxUint64 GetTotalRead() const;

private:
	void ReadHeaders();
	void InitEncoding();
	void SetError(const wxString& error, int code = -1);
	void SendDataReadyEvent(wxMemoryBuffer& data);

private:
	std::unique_ptr<Request> mRequest;
	asio::ip::tcp::socket* mSocket{};
	asio::io_service* mIOService{};
	HeadersContainer mHeaders;
	std::unique_ptr<wxMBConv> mConverter;
	size_t mStatusCode = 0;
	wxString mStatusMessage;
	wxString mError;
	wxUint64 mSize = 0;
	wxUint64 mTotalRead = 0;
	bool mReadFinished{};

	wxMemoryBuffer mCachedData;
};

struct ReplyDataReadyEvent;
wxDECLARE_EVENT(EVT_REPLY_DATA_READY, ReplyDataReadyEvent);
struct ReplyDataReadyEvent : public wxCommandEvent
{
	wxMemoryBuffer data;
	bool readFinished = false;

	ReplyDataReadyEvent(const wxMemoryBuffer& data)
		: wxCommandEvent(EVT_REPLY_DATA_READY, 0)
		, data{data}
	{}
	ReplyDataReadyEvent(const ReplyDataReadyEvent& other)
		: wxCommandEvent(other)
		, data{other.data}
		, readFinished{other.readFinished}
	{}

	virtual wxEvent* Clone() const override { return new ReplyDataReadyEvent(*this); }

	NO_ASSIGN_CLASS(ReplyDataReadyEvent);
};

} // namespace Net
