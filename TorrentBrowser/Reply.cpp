#include "stdafx.h"
#include "Reply.h"
#include "Net/Request.h"
#include "Net/UrlUtils.h"

using asio::ip::tcp;

namespace Net {
;
wxMBConv* CreateConverter(const wxString& charset)
{
	DCHECK_WXSTRING(charset);

	wxFontEncoding encoding = wxFONTENCODING_UTF8;
	if (charset.CmpNoCase(_S("koi8-r")) == 0)
		encoding = wxFONTENCODING_KOI8;
	else if (charset.CmpNoCase(_S("windows-1251")) == 0)
		encoding = wxFONTENCODING_CP1251;

	return new wxConvAuto{encoding};
}

Reply::Reply(const Request& request, tcp::socket* takedSocket, asio::io_service* takedIOService)
	: mRequest{std::make_unique<Request>(request)}
	, mSocket{takedSocket}
	, mIOService{takedIOService}
{
	ReadHeaders();

	wxString headerValue = mHeaders.GetHeader(KnownHeaders::ContentLength);
	if (!headerValue.IsEmpty()) {
		long long len;
		if (headerValue.ToLongLong(&len))
			mSize = range_cast<wxUint64>(len);
	}
}

Reply::~Reply()
{
	wxDELETE(mSocket);
	wxDELETE(mIOService);
}

void Reply::ReadHeaders()
{
	const std::string endl = "\r\n";

	wxString urlStr = mRequest->GetUrl().BuildUnescapedURI();

	asio::streambuf response;

	// Read the response status line. The response streambuf will automatically
	// grow to accommodate the entire line. The growth may be limited by passing
	// a maximum size to the streambuf constructor.
	asio::error_code ec;
	asio::read_until(*mSocket, response, endl, ec);
	if (ec) {
		SetError(wxString::Format(_S("Can't read response status line for '%s' error = '%s' code = '%d'"), 
			urlStr, ec.message().c_str(), ec.value()));
		return;
	}
	// Check that response is OK.
	std::istream response_stream(&response);
	std::string http_version;
	response_stream >> http_version;
	response_stream >> mStatusCode;
	std::string status_message;
	std::getline(response_stream, status_message);
	mStatusMessage = status_message;
	boost::algorithm::trim(status_message);
	if (!response_stream || http_version.substr(0, 5) != "HTTP/") {
		SetError(wxString::Format(_S("Invalid response for %s"), mRequest->GetUrl().BuildUnescapedURI()));
		return;
	}

	if (mStatusCode != 200) {
		wxLogWarning("Response: code = %u message = %s url = %s", mStatusCode, status_message, mRequest->GetUrl().BuildUnescapedURI());
	}

	// Read the response headers, which are terminated by a blank line.
	asio::read_until(*mSocket, response, endl + endl, ec);
	if (ec) {
		SetError(wxString::Format(_S("Can't read response headers for '%s' error = '%s' code = '%d'"), urlStr, ec.message().c_str(), ec.value()));
		return;
	}

	// Process the response headers.
	std::string headerLine;
	while (std::getline(response_stream, headerLine) && headerLine != "\r") {
		size_t pos = headerLine.find(':');
		if (pos == std::string::npos)
			continue;;
		wxString header = headerLine.substr(0, pos);
		wxString value = headerLine.substr(pos + 1);
		value.Trim(true).Trim(false);

		mHeaders.SetHeader(header, value);
	}
	
	if (300 <= mStatusCode && mStatusCode < 400) {	// Redirection
		return;
	}

	if (100 <= mStatusCode && mStatusCode < 299) {
		InitEncoding();

		size_t size = response.size();
		if (size > 0) {
			char* buff = (char*)mCachedData.GetWriteBuf(size);
			response.sgetn(buff, size);
			mCachedData.UngetWriteBuf(size);
			mTotalRead = size;

			asio::error_code ec;
			if (mSocket->available(ec) == 0)
				mReadFinished = true; // TODO: why read finished? May be just no data
		}

		return;
	}

	SetError(wxString::Format(_S("Unknown response code = %u"), mStatusCode));
}

size_t Reply::ReadData(size_t readCount)
{
	size_t currRead = mCachedData.GetDataLen();

	if (mCachedData.GetDataLen() > 0) {
		SendDataReadyEvent(mCachedData);
		mCachedData.Clear();
	}

	if (mReadFinished || (readCount != UINT_MAX && currRead >= readCount)) {
		mReadFinished = true;
		SendDataReadyEvent(mCachedData);
		return currRead;
	}

	mReadFinished = false;

	CHECK_RET_VAL(mSocket->is_open(), currRead);

	auto WaitForData = [&](bool infinite) -> boost::optional<size_t> {
		const size_t timeout = 1000;
		const size_t waitStep = 2;
		size_t totalWait = 0;

		asio::error_code ec;
		size_t available = 0;
		while (available == 0) {
			available = mSocket->available(ec);
			if (ec) {
				SetError(wxString::Format(_S("basic_socket::available error: '%s' code = '%d'"), ec.message().c_str(), ec.value()));
				return boost::none;
			}

			wxMilliSleep(waitStep);
			if (!infinite) {
				totalWait += waitStep;
				if (totalWait > timeout)
					return boost::none;
			}
		}

		return available;
	};

	for (;;) {
		asio::error_code ec;
		size_t available = mSocket->available(ec);
		if (ec) {
			SetError(wxString::Format(_S("basic_socket::available error: '%s' code = '%d'"), ec.message().c_str(), ec.value()));
			break;
		}
		if (available == 0) {
			if (0 < mSize && mSize < UINT_MAX) {
				if (mTotalRead >= mSize) {
					mReadFinished = true;
					break;
				}
				else {
					boost::optional<size_t> avail = WaitForData(true);
					if (!avail)
						break;
				}
			}
			else {
				boost::optional<size_t> avail = WaitForData(false);
				if (!avail) {
					mReadFinished = true;
					break;
				}
			}
		}

		if (readCount != UINT_MAX && currRead + available > readCount)
			available = (size_t)std::abs((int)currRead - (int)readCount);
			
		wxMemoryBuffer data{available};
		void* buff = data.GetWriteBuf(available);
		
		size_t read = mSocket->read_some(asio::buffer(buff, available), ec);
		currRead += read;
		mTotalRead += read;
		if (ec && ec != asio::error::eof) {
			SetError(ec.message(), ec.value());
			break;
		}

		data.UngetWriteBuf(read);

		if (read != 0) {
			SendDataReadyEvent(data);
		}

		if (readCount != UINT_MAX && readCount == currRead) {
			return currRead;
		}

		if (ec == asio::error::eof) {
			mReadFinished = true;
			break;
		}
	}

	mReadFinished = true;
	SendDataReadyEvent(mCachedData);

	return currRead;
}

size_t Reply::ReadDataUntil(const wxString& pattern)
{
	size_t currRead = mCachedData.GetDataLen();

	if (!mCachedData.IsEmpty()) {
		size_t len = mCachedData.GetDataLen();
		wxString cached{mCachedData, len};
		int ind = cached.Find(pattern);
		if (ind != -1) {
			ind += pattern.Len();
			cached.erase((size_t)ind);
			mCachedData.Clear();
			len = cached.Len();
			mCachedData.AppendData(wxStringBuffer(cached, len), len);
		}

		mTotalRead = len;

		if (!mReadFinished)
			mReadFinished = ind != -1;
		SendDataReadyEvent(mCachedData);

		if (ind != -1)
			return len;
	}

	if (mReadFinished)
		return currRead;

	CHECK_RET_VAL(mSocket->is_open(), false);

	asio::streambuf response;
	asio::error_code ec;
	asio::read_until(*mSocket, response, pattern.ToStdString(), ec);
	wxMemoryBuffer data;
	if (ec != asio::error::eof) {
		asio::streambuf::const_buffers_type rdata = response.data();

		size_t size = response.size();
		char* buff = (char*)data.GetWriteBuf(size);
		response.sgetn(buff, size);
		data.UngetWriteBuf(size);
		mTotalRead += size;
	}

	mReadFinished = true;
	SendDataReadyEvent(data);

	return currRead;
}

HeadersContainer& Reply::GetHeaders()
{
	return mHeaders;
}

const HeadersContainer& Reply::GetHeaders() const
{
	return mHeaders;
}

const Request& Reply::GetRequest() const
{
	return *mRequest;
}

void Reply::InitEncoding()
{
	wxString charset;
	wxString content = mHeaders.GetHeader(KnownHeaders::ContentType);
	if (!content.IsEmpty()) {
		wxStringTokenizer tokenizer(content, _S(";"));
		while (tokenizer.HasMoreTokens()) {
			wxString token = tokenizer.GetNextToken();
			token.Trim(true).Trim(false);
			if (token.StartsWith(_S("charset="), &charset)) {
				break;
			}
		}
	}

	if (charset.IsEmpty())
		charset = _S("UTF-8");

	mConverter.reset(CreateConverter(charset));
}

size_t Reply::GetStatuseCode() const
{
	return mStatusCode;
}

const wxString Reply::GetError() const
{
	return mError;
}

void Reply::SetError(const wxString& error, int code)
{
	mError = wxString::Format(_S("Error: '%s'"), error);
	if (code != -1)
		mError += wxString::Format(_S("code: '%d'"), code);
	wxLogError(mError);
}

boost::optional<wxURL> Reply::GetRedirectUrl() const
{
	boost::optional<wxURL> result;

	if (300 <= mStatusCode && mStatusCode < 400) {
		wxString redirectUrlStr = mHeaders.GetHeader(KnownHeaders::Location);
		if (!redirectUrlStr.IsEmpty()) {
			redirectUrlStr.Trim(true).Trim(false);
			result = UrlUtils::MakeUrl(redirectUrlStr);
		}
	}
	
	return result;
}

const wxMBConv& Reply::GetConverter() const
{
	return *mConverter;
}

wxString Reply::GetFilename() const
{
	wxString filename;

	wxString headerValue = mHeaders.GetHeader(KnownHeaders::ContentDisposition);
	if (!headerValue.IsEmpty()) {
		wxString regexp = wxS(".*;.*filename=(.*)\\s*");
		wxRegEx regex{regexp};
		DCHECK(regex.IsValid());
		if (regex.Matches(headerValue) && regex.GetMatchCount() > 1) {
			filename = regex.GetMatch(headerValue, 1);
			filename.Trim().Trim(true);
			filename.Replace(wxS(";"), wxEmptyString);
			filename.Replace(wxS("\""), wxEmptyString);
		}
	}

	return filename;
}

wxUint64 Reply::GetResourceSize() const
{
	return mSize;
}

size_t Reply::GetReceiveBufferSize() const
{
	size_t blockSize = 8 * 1024;
	asio::socket_base::receive_buffer_size option;
	mSocket->get_option(option);
	int currBuffSize = option.value();
	if (currBuffSize > 0)
		blockSize = range_cast<size_t>(currBuffSize);

	return blockSize;
}

bool Reply::IsReadFinished() const
{
	return mReadFinished;
}

wxUint64 Reply::GetTotalRead() const
{
	return mTotalRead;
}

void Reply::SendDataReadyEvent(wxMemoryBuffer& data)
{
	ReplyDataReadyEvent evt{data};
	evt.readFinished = mReadFinished;
	evt.SetEventObject(this);
	ProcessEvent(evt);
}

wxDEFINE_EVENT(EVT_REPLY_DATA_READY, ReplyDataReadyEvent);

} // namespace Net
