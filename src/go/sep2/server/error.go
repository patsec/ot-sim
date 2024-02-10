package server

import (
	"fmt"
	"net/http"
)

type WebError struct {
	Cause   error
	Status  int
	Message string
}

func NewWebError(cause error) *WebError {
	err := &WebError{
		Cause:  cause,
		Status: http.StatusBadRequest,
	}

	return err
}

func (this *WebError) SetStatus(status int) *WebError {
	this.Status = status
	return this
}

func (this *WebError) SetMessage(msg string) *WebError {
	this.Message = msg
	return this
}

func (this *WebError) SetMessageToError() *WebError {
	this.Message = this.Cause.Error()
	return this
}

func (this WebError) Error() string {
	return this.Cause.Error()
}

type ErrorHandler func(http.ResponseWriter, *http.Request) error

func (this ErrorHandler) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	if err := this(w, r); err != nil {
		web, ok := err.(*WebError)
		if !ok {
			w.WriteHeader(http.StatusInternalServerError)
			return
		}

		fmt.Printf("[ERROR] %v\n", web)

		w.WriteHeader(web.Status)

		if web.Message != "" {
			w.Write([]byte(web.Message))
		}
	}
}
